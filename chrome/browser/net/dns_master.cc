// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_master.h"

#include <algorithm>
#include <set>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/lock.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"

using base::TimeDelta;

namespace chrome_browser_net {

class DnsMaster::LookupRequest {
 public:
  LookupRequest(DnsMaster* master,
                net::HostResolver* host_resolver,
                const std::string& hostname)
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          net_callback_(this, &LookupRequest::OnLookupFinished)),
        master_(master),
        hostname_(hostname),
        resolver_(host_resolver) {
  }

  // Return underlying network resolver status.
  // net::OK ==> Host was found synchronously.
  // net:ERR_IO_PENDING ==> Network will callback later with result.
  // anything else ==> Host was not found synchronously.
  int Start() {
    // Port doesn't really matter.
    net::HostResolver::RequestInfo resolve_info(hostname_, 80);

    // Make a note that this is a speculative resolve request. This allows us
    // to separate it from real navigations in the observer's callback, and
    // lets the HostResolver know it can de-prioritize it.
    resolve_info.set_is_speculative(true);
    return resolver_.Resolve(resolve_info, &addresses_, &net_callback_);
  }

 private:
  void OnLookupFinished(int result) {
    master_->OnLookupFinished(this, hostname_, result == net::OK);
  }

  // HostResolver will call us using this callback when resolution is complete.
  net::CompletionCallbackImpl<LookupRequest> net_callback_;

  DnsMaster* master_;  // Master which started us.

  const std::string hostname_;  // Hostname to resolve.
  net::SingleRequestHostResolver resolver_;
  net::AddressList addresses_;

  DISALLOW_COPY_AND_ASSIGN(LookupRequest);
};

DnsMaster::DnsMaster(net::HostResolver* host_resolver,
                     MessageLoop* host_resolver_loop,
                     TimeDelta max_queue_delay,
                     size_t max_concurrent)
  : peak_pending_lookups_(0),
    shutdown_(false),
    max_concurrent_lookups_(max_concurrent),
    max_queue_delay_(max_queue_delay),
    host_resolver_(host_resolver),
    host_resolver_loop_(host_resolver_loop) {
}

DnsMaster::~DnsMaster() {
  DCHECK(shutdown_);
}

void DnsMaster::Shutdown() {
  AutoLock auto_lock(lock_);

  DCHECK(!shutdown_);
  shutdown_ = true;

  std::set<LookupRequest*>::iterator it;
  for (it = pending_lookups_.begin(); it != pending_lookups_.end(); ++it)
    delete *it;
}

// Overloaded Resolve() to take a vector of names.
void DnsMaster::ResolveList(const NameList& hostnames,
                            DnsHostInfo::ResolutionMotivation motivation) {
  AutoLock auto_lock(lock_);

  // We need to run this on |host_resolver_loop_| since we may access
  // |host_resolver_| which is not thread safe.
  if (MessageLoop::current() != host_resolver_loop_) {
    host_resolver_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DnsMaster::ResolveList, hostnames, motivation));
    return;
  }

  NameList::const_iterator it;
  for (it = hostnames.begin(); it < hostnames.end(); ++it)
    PreLockedResolve(*it, motivation);
}

// Basic Resolve() takes an invidual name, and adds it
// to the queue.
void DnsMaster::Resolve(const std::string& hostname,
                        DnsHostInfo::ResolutionMotivation motivation) {
  if (0 == hostname.length())
    return;
  AutoLock auto_lock(lock_);

  // We need to run this on |host_resolver_loop_| since we may access
  // |host_resolver_| which is not thread safe.
  if (MessageLoop::current() != host_resolver_loop_) {
    host_resolver_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DnsMaster::Resolve, hostname, motivation));
    return;
  }

  PreLockedResolve(hostname, motivation);
}

bool DnsMaster::AccruePrefetchBenefits(const GURL& referrer,
                                      DnsHostInfo* navigation_info) {
  std::string hostname = navigation_info->hostname();

  AutoLock auto_lock(lock_);
  Results::iterator it = results_.find(hostname);
  if (it == results_.end()) {
    // Remain under lock to assure static HISTOGRAM constructor is safely run.
    // Use UMA histogram to quantify potential future gains here.
    UMA_HISTOGRAM_LONG_TIMES("DNS.UnexpectedResolutionL",
                             navigation_info->resolve_duration());
    navigation_info->DLogResultsStats("DNS UnexpectedResolution");

    NonlinkNavigation(referrer, navigation_info);
    return false;
  }
  DnsHostInfo& prefetched_host_info(it->second);

  // Sometimes a host is used as a subresource by several referrers, so it is
  // in our list, but was never motivated by a page-link-scan.  In that case, it
  // really is an "unexpected" navigation, and we should tally it, and augment
  // our referrers_.
  bool referrer_based_prefetch = !prefetched_host_info.was_linked();
  if (referrer_based_prefetch) {
    // This wasn't the first time this host refered to *some* referrer.
    NonlinkNavigation(referrer, navigation_info);
  }

  DnsBenefit benefit = prefetched_host_info.AccruePrefetchBenefits(
      navigation_info);
  switch (benefit) {
    case PREFETCH_NAME_FOUND:
    case PREFETCH_NAME_NONEXISTANT:
      // Remain under lock to push data.
      cache_hits_.push_back(*navigation_info);
      if (referrer_based_prefetch) {
        std::string motivating_referrer(
            prefetched_host_info.referring_hostname());
        if (!motivating_referrer.empty()) {
          referrers_[motivating_referrer].AccrueValue(
              navigation_info->benefits_remaining(), hostname);
        }
      }
      return true;

    case PREFETCH_CACHE_EVICTION:
      // Remain under lock to push data.
      cache_eviction_map_[hostname] = *navigation_info;
      return false;

    case PREFETCH_NO_BENEFIT:
      // Prefetch never hit the network. Name was pre-cached.
      return false;

    default:
      DCHECK(false);
      return false;
  }
}

void DnsMaster::NonlinkNavigation(const GURL& referrer,
                                  DnsHostInfo* navigation_info) {
  std::string referring_host = referrer.host();
  if (referring_host.empty() || referring_host == navigation_info->hostname())
    return;

  referrers_[referring_host].SuggestHost(navigation_info->hostname());
}

void DnsMaster::NavigatingTo(const std::string& host_name) {
  AutoLock auto_lock(lock_);

  // We need to run this on |host_resolver_loop_| since we may access
  // |host_resolver_| which is not thread safe.
  if (MessageLoop::current() != host_resolver_loop_) {
    host_resolver_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &DnsMaster::NavigatingTo, host_name));
    return;
  }

  Referrers::iterator it = referrers_.find(host_name);
  if (referrers_.end() == it)
    return;
  Referrer* referrer = &(it->second);
  for (Referrer::iterator future_host = referrer->begin();
       future_host != referrer->end(); ++future_host) {
    DnsHostInfo* queued_info = PreLockedResolve(
        future_host->first,
        DnsHostInfo::LEARNED_REFERAL_MOTIVATED);
    if (queued_info)
      queued_info->SetReferringHostname(host_name);
  }
}

// Provide sort order so all .com's are together, etc.
struct RightToLeftStringSorter {
  bool operator()(const std::string& left, const std::string& right) const {
    if (left == right) return true;
    size_t left_already_matched = left.size();
    size_t right_already_matched = right.size();

    // Ensure both strings have characters.
    if (!left_already_matched) return true;
    if (!right_already_matched) return false;

    // Watch for trailing dot, so we'll always be safe to go one beyond dot.
    if ('.' == left[left.size() - 1]) {
      if ('.' != right[right.size() - 1])
        return true;
      // Both have dots at end of string.
      --left_already_matched;
      --right_already_matched;
    } else {
      if ('.' == right[right.size() - 1])
        return false;
    }

    while (1) {
      if (!left_already_matched) return true;
      if (!right_already_matched) return false;

      size_t left_length, right_length;
      size_t left_start = left.find_last_of('.', left_already_matched - 1);
      if (std::string::npos == left_start) {
        left_length = left_already_matched;
        left_already_matched = left_start = 0;
      } else {
        left_length = left_already_matched - left_start;
        left_already_matched = left_start;
        ++left_start;  // Don't compare the dot.
      }
      size_t right_start = right.find_last_of('.', right_already_matched - 1);
      if (std::string::npos == right_start) {
        right_length = right_already_matched;
        right_already_matched = right_start = 0;
      } else {
        right_length = right_already_matched - right_start;
        right_already_matched = right_start;
        ++right_start;  // Don't compare the dot.
      }

      int diff = left.compare(left_start, left.size(),
                              right, right_start, right.size());
      if (diff > 0) return false;
      if (diff < 0) return true;
    }
  }
};

void DnsMaster::GetHtmlReferrerLists(std::string* output) {
  AutoLock auto_lock(lock_);
  if (referrers_.empty())
    return;

  // TODO(jar): Remove any plausible JavaScript from names before displaying.

  typedef std::set<std::string, struct RightToLeftStringSorter> SortedNames;
  SortedNames sorted_names;

  for (Referrers::iterator it = referrers_.begin();
       referrers_.end() != it; ++it)
    sorted_names.insert(it->first);

  output->append("<br><table border>");
  StringAppendF(output,
      "<tr><th>%s</th><th>%s</th></tr>",
      "Host for Page", "Host(s) in Page<br>(benefits in ms)");

  for (SortedNames::iterator it = sorted_names.begin();
       sorted_names.end() != it; ++it) {
    Referrer* referrer = &(referrers_[*it]);
    StringAppendF(output, "<tr align=right><td>%s</td><td>", it->c_str());
    output->append("<table>");
    for (Referrer::iterator future_host = referrer->begin();
         future_host != referrer->end(); ++future_host) {
      StringAppendF(output, "<tr align=right><td>(%dms)</td><td>%s</td></tr>",
          static_cast<int>(future_host->second.latency().InMilliseconds()),
          future_host->first.c_str());
    }
    output->append("</table></td></tr>");
  }
  output->append("</table>");
}

void DnsMaster::GetHtmlInfo(std::string* output) {
  // Local lists for calling DnsHostInfo
  DnsHostInfo::DnsInfoTable cache_hits;
  DnsHostInfo::DnsInfoTable cache_evictions;
  DnsHostInfo::DnsInfoTable name_not_found;
  DnsHostInfo::DnsInfoTable network_hits;
  DnsHostInfo::DnsInfoTable already_cached;

  // Get copies of all useful data under protection of a lock.
  typedef std::map<std::string, DnsHostInfo, RightToLeftStringSorter> Snapshot;
  Snapshot snapshot;
  {
    AutoLock auto_lock(lock_);
    // DnsHostInfo supports value semantics, so we can do a shallow copy.
    for (Results::iterator it(results_.begin()); it != results_.end(); it++) {
      snapshot[it->first] = it->second;
    }
    for (Results::iterator it(cache_eviction_map_.begin());
         it != cache_eviction_map_.end();
         it++) {
      cache_evictions.push_back(it->second);
    }
    // Reverse list as we copy cache hits, so that new hits are at the top.
    size_t index = cache_hits_.size();
    while (index > 0) {
      index--;
      cache_hits.push_back(cache_hits_[index]);
    }
  }

  // Partition the DnsHostInfo's into categories.
  for (Snapshot::iterator it(snapshot.begin()); it != snapshot.end(); it++) {
    if (it->second.was_nonexistant()) {
      name_not_found.push_back(it->second);
      continue;
    }
    if (!it->second.was_found())
      continue;  // Still being processed.
    if (TimeDelta() != it->second.benefits_remaining()) {
      network_hits.push_back(it->second);  // With no benefit yet.
      continue;
    }
    if (DnsHostInfo::kMaxNonNetworkDnsLookupDuration >
      it->second.resolve_duration()) {
      already_cached.push_back(it->second);
      continue;
    }
    // Remaining case is where prefetch benefit was significant, and was used.
    // Since we shot those cases as historical hits, we won't bother here.
  }

  bool brief = false;
#ifdef NDEBUG
  brief = true;
#endif  // NDEBUG

  // Call for display of each table, along with title.
  DnsHostInfo::GetHtmlTable(cache_hits,
      "Prefetching DNS records produced benefits for ", false, output);
  DnsHostInfo::GetHtmlTable(cache_evictions,
      "Cache evictions negated DNS prefetching benefits for ", brief, output);
  DnsHostInfo::GetHtmlTable(network_hits,
      "Prefetching DNS records was not yet beneficial for ", brief, output);
  DnsHostInfo::GetHtmlTable(already_cached,
      "Previously cached resolutions were found for ", brief, output);
  DnsHostInfo::GetHtmlTable(name_not_found,
      "Prefetching DNS records revealed non-existance for ", brief, output);
}

DnsHostInfo* DnsMaster::PreLockedResolve(
    const std::string& hostname,
    DnsHostInfo::ResolutionMotivation motivation) {
  // DCHECK(We have the lock);
  DCHECK_NE(0u, hostname.length());

  if (shutdown_)
    return NULL;

  DnsHostInfo* info = &results_[hostname];
  info->SetHostname(hostname);  // Initialize or DCHECK.
  // TODO(jar):  I need to discard names that have long since expired.
  // Currently we only add to the domain map :-/

  DCHECK(info->HasHostname(hostname));

  if (!info->NeedsDnsUpdate(hostname)) {
    info->DLogResultsStats("DNS PrefetchNotUpdated");
    return NULL;
  }

  info->SetQueuedState(motivation);
  work_queue_.Push(hostname, motivation);
  PreLockedScheduleLookups();
  return info;
}

void DnsMaster::PreLockedScheduleLookups() {
  // We need to run this on |host_resolver_loop_| since we may access
  // |host_resolver_| which is not thread safe.
  DCHECK_EQ(MessageLoop::current(), host_resolver_loop_);

  while (!work_queue_.IsEmpty() &&
         pending_lookups_.size() < max_concurrent_lookups_) {
    const std::string hostname(work_queue_.Pop());
    DnsHostInfo* info = &results_[hostname];
    DCHECK(info->HasHostname(hostname));
    info->SetAssignedState();

    if (PreLockedCongestionControlPerformed(info)) {
      DCHECK(work_queue_.IsEmpty());
      return;
    }

    LookupRequest* request = new LookupRequest(this, host_resolver_, hostname);
    int status = request->Start();
    if (status == net::ERR_IO_PENDING) {
      // Will complete asynchronously.
      pending_lookups_.insert(request);
      peak_pending_lookups_ = std::max(peak_pending_lookups_,
                                       pending_lookups_.size());
    } else {
      // Completed synchronously (was already cached by HostResolver), or else
      // there was (equivalently) some network error that prevents us from
      // finding the name.  Status net::OK means it was "found."
      PrelockedLookupFinished(request, hostname, status == net::OK);
      delete request;
    }
  }
}

bool DnsMaster::PreLockedCongestionControlPerformed(DnsHostInfo* info) {
  // Note: queue_duration is ONLY valid after we go to assigned state.
  if (info->queue_duration() < max_queue_delay_)
    return false;
  // We need to discard all entries in our queue, as we're keeping them waiting
  // too long.  By doing this, we'll have a chance to quickly service urgent
  // resolutions, and not have a bogged down system.
  while (true) {
    info->RemoveFromQueue();
    if (work_queue_.IsEmpty())
      break;
    info = &results_[work_queue_.Pop()];
    info->SetAssignedState();
  }
  return true;
}

void DnsMaster::OnLookupFinished(LookupRequest* request,
                                 const std::string& hostname, bool found) {
  DCHECK_EQ(MessageLoop::current(), host_resolver_loop_);

  AutoLock auto_lock(lock_);  // For map access (changing info values).
  PrelockedLookupFinished(request, hostname, found);
  pending_lookups_.erase(request);
  delete request;

  PreLockedScheduleLookups();
}

void DnsMaster::PrelockedLookupFinished(LookupRequest* request,
                                        const std::string& hostname,
                                        bool found) {
  DnsHostInfo* info = &results_[hostname];
  DCHECK(info->HasHostname(hostname));
  if (info->is_marked_to_delete()) {
    results_.erase(hostname);
  } else {
    if (found)
      info->SetFoundState();
    else
      info->SetNoSuchNameState();
  }
}

void DnsMaster::DiscardAllResults() {
  AutoLock auto_lock(lock_);
  // Delete anything listed so far in this session that shows in about:dns.
  cache_eviction_map_.clear();
  cache_hits_.clear();
  referrers_.clear();


  // Try to delete anything in our work queue.
  while (!work_queue_.IsEmpty()) {
    // Emulate processing cycle as though host was not found.
    std::string hostname = work_queue_.Pop();
    DnsHostInfo* info = &results_[hostname];
    DCHECK(info->HasHostname(hostname));
    info->SetAssignedState();
    info->SetNoSuchNameState();
  }
  // Now every result_ is either resolved, or is being resolved
  // (see LookupRequest).

  // Step through result_, recording names of all hosts that can't be erased.
  // We can't erase anything being worked on.
  Results assignees;
  for (Results::iterator it = results_.begin(); results_.end() != it; ++it) {
    std::string hostname = it->first;
    DnsHostInfo* info = &it->second;
    DCHECK(info->HasHostname(hostname));
    if (info->is_assigned()) {
      info->SetPendingDeleteState();
      assignees[hostname] = *info;
    }
  }
  DCHECK(assignees.size() <= max_concurrent_lookups_);
  results_.clear();
  // Put back in the names being worked on.
  for (Results::iterator it = assignees.begin(); assignees.end() != it; ++it) {
    DCHECK(it->second.is_marked_to_delete());
    results_[it->first] = it->second;
  }
}

void DnsMaster::TrimReferrers() {
  std::vector<std::string> hosts;
  AutoLock auto_lock(lock_);
  for (Referrers::const_iterator it = referrers_.begin();
       it != referrers_.end(); ++it)
    hosts.push_back(it->first);
  for (size_t i = 0; i < hosts.size(); ++i)
    if (!referrers_[hosts[i]].Trim())
      referrers_.erase(hosts[i]);
}

void DnsMaster::SerializeReferrers(ListValue* referral_list) {
  referral_list->Clear();
  AutoLock auto_lock(lock_);
  for (Referrers::const_iterator it = referrers_.begin();
       it != referrers_.end(); ++it) {
    // Serialize the list of subresource names.
    Value* subresource_list(it->second.Serialize());

    // Create a list for each referer.
    ListValue* motivating_host(new ListValue);
    motivating_host->Append(new StringValue(it->first));
    motivating_host->Append(subresource_list);

    referral_list->Append(motivating_host);
  }
}

void DnsMaster::DeserializeReferrers(const ListValue& referral_list) {
  AutoLock auto_lock(lock_);
  for (size_t i = 0; i < referral_list.GetSize(); ++i) {
    ListValue* motivating_host;
    if (!referral_list.GetList(i, &motivating_host))
      continue;
    std::string motivating_referrer;
    if (!motivating_host->GetString(0, &motivating_referrer))
      continue;
    Value* subresource_list;
    if (!motivating_host->Get(1, &subresource_list))
      continue;
    if (motivating_referrer.empty())
      continue;
    referrers_[motivating_referrer].Deserialize(*subresource_list);
  }
}


//------------------------------------------------------------------------------

DnsMaster::HostNameQueue::HostNameQueue() {
}

DnsMaster::HostNameQueue::~HostNameQueue() {
}

void DnsMaster::HostNameQueue::Push(const std::string& hostname,
    DnsHostInfo::ResolutionMotivation motivation) {
  switch (motivation) {
    case DnsHostInfo::STATIC_REFERAL_MOTIVATED:
    case DnsHostInfo::LEARNED_REFERAL_MOTIVATED:
    case DnsHostInfo::MOUSE_OVER_MOTIVATED:
      rush_queue_.push(hostname);
      break;

    default:
      background_queue_.push(hostname);
      break;
  }
}

bool DnsMaster::HostNameQueue::IsEmpty() const {
  return rush_queue_.empty() && background_queue_.empty();
}

std::string DnsMaster::HostNameQueue::Pop() {
  DCHECK(!IsEmpty());
  if (!rush_queue_.empty()) {
    std::string hostname(rush_queue_.front());
    rush_queue_.pop();
    return hostname;
  }
  std::string hostname(background_queue_.front());
  background_queue_.pop();
  return hostname;
}

}  // namespace chrome_browser_net
