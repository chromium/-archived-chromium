// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_master.h"

#include <algorithm>
#include <sstream>

#include "base/histogram.h"
#include "base/lock.h"
#include "base/revocable_store.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"

namespace chrome_browser_net {

// static
const size_t DnsMaster::kMaxConcurrentLookups = 8;

class DnsMaster::LookupRequest : RevocableStore::Revocable {
 public:
  LookupRequest(DnsMaster* master, const std::string& hostname)
      : RevocableStore::Revocable(&master->pending_callbacks_),
        callback_(this, &LookupRequest::OnLookupFinished),
        hostname_(hostname),
        master_(master) {
  }

  bool Start() {
    const int result = resolver_.Resolve(hostname_, 80, &addresses_,
                                         &callback_);
    return (result == net::ERR_IO_PENDING);
  }

 private:
  void OnLookupFinished(int result) {
    if (revoked()) {
      delete this;
      return;
    }

    if (result == net::OK)
      master_->SetFoundState(hostname_);
    else
      master_->SetNoSuchNameState(hostname_);

    delete this;
  }

  net::CompletionCallbackImpl<LookupRequest> callback_;
  const std::string hostname_;
  net::HostResolver resolver_;
  net::AddressList addresses_;
  DnsMaster* master_;
};

DnsMaster::DnsMaster() : pending_lookups_(0), peak_pending_lookups_(0) {
}

DnsMaster::~DnsMaster() {
  pending_callbacks_.RevokeAll();
}

// Overloaded Resolve() to take a vector of names.
void DnsMaster::ResolveList(const NameList& hostnames,
                            DnsHostInfo::ResolutionMotivation motivation) {
  AutoLock auto_lock(lock_);

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
    UMA_HISTOGRAM_LONG_TIMES(L"DNS.UnexpectedResolutionL",
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
    if (base::TimeDelta() != it->second.benefits_remaining()) {
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
  DCHECK(0 != hostname.length());

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
  name_buffer_.push(hostname);

  PreLockedScheduleLookups();

  return info;
}

void DnsMaster::PreLockedScheduleLookups() {
  while (!name_buffer_.empty() && pending_lookups_ < kMaxConcurrentLookups) {
    const std::string hostname(name_buffer_.front());
    name_buffer_.pop();

    DnsHostInfo* info = &results_[hostname];
    DCHECK(info->HasHostname(hostname));
    info->SetAssignedState();

    LookupRequest* request = new LookupRequest(this, hostname);
    if (request->Start()) {
      pending_lookups_++;
      peak_pending_lookups_ = std::max(peak_pending_lookups_, pending_lookups_);
    } else {
      NOTREACHED();
      delete request;
    }
  }
}

void DnsMaster::SetFoundState(const std::string hostname) {
  AutoLock auto_lock(lock_);  // For map access (changing info values).
  DnsHostInfo* info = &results_[hostname];
  DCHECK(info->HasHostname(hostname));
  if (info->is_marked_to_delete())
    results_.erase(hostname);
  else
    info->SetFoundState();

  pending_lookups_--;
  PreLockedScheduleLookups();
}

void DnsMaster::SetNoSuchNameState(const std::string hostname) {
  AutoLock auto_lock(lock_);  // For map access (changing info values).
  DnsHostInfo* info = &results_[hostname];
  DCHECK(info->HasHostname(hostname));
  if (info->is_marked_to_delete())
    results_.erase(hostname);
  else
    info->SetNoSuchNameState();

  pending_lookups_--;
  PreLockedScheduleLookups();
}

void DnsMaster::DiscardAllResults() {
  AutoLock auto_lock(lock_);
  // Delete anything listed so far in this session that shows in about:dns.
  cache_eviction_map_.clear();
  cache_hits_.clear();
  referrers_.clear();


  // Try to delete anything in our work queue.
  while (!name_buffer_.empty()) {
    // Emulate processing cycle as though host was not found.
    std::string hostname = name_buffer_.front();
    name_buffer_.pop();
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
  DCHECK(assignees.size() <= kMaxConcurrentLookups);
  results_.clear();
  // Put back in the names being worked on.
  for (Results::iterator it = assignees.begin(); assignees.end() != it; ++it) {
    DCHECK(it->second.is_marked_to_delete());
    results_[it->first] = it->second;
  }
}

}  // namespace chrome_browser_net
