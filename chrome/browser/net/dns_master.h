// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A DnsMaster object is instantiated once in the browser
// process, and manages asynchronous resolution of DNS hostnames.
// Most hostname lists are sent out by renderer processes, and
// involve lists of hostnames that *might* be used in the near
// future by the browsing user.  The goal of this class is to
// cause the underlying DNS structure to lookup a hostname before
// it is really needed, and hence reduce latency in the standard
// lookup paths.

#ifndef CHROME_BROWSER_NET_DNS_MASTER_H_
#define CHROME_BROWSER_NET_DNS_MASTER_H_

#include <map>
#include <queue>
#include <set>
#include <string>

#include "base/lock.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/common/net/dns.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace chrome_browser_net {

typedef chrome_common_net::NameList NameList;
typedef std::map<std::string, DnsHostInfo> Results;

class DnsMaster {
 public:
  // Too many concurrent lookups negate benefits of prefetching
  // by trashing the OS cache.
  static const size_t kMaxConcurrentLookups;

  DnsMaster();
  ~DnsMaster();

  // Cancel pending requests and prevent new ones from being made.
  void Shutdown();

  // In some circumstances, for privacy reasons, all results should be
  // discarded.  This method gracefully handles that activity.
  // Destroy all our internal state, which shows what names we've looked up, and
  // how long each has taken, etc. etc.  We also destroy records of suggesses
  // (cache hits etc.).
  void DiscardAllResults();

  // Add hostname(s) to the queue for processing.
  void ResolveList(const NameList& hostnames,
                   DnsHostInfo::ResolutionMotivation motivation);
  void Resolve(const std::string& hostname,
               DnsHostInfo::ResolutionMotivation motivation);

  // Get latency benefit of the prefetch that we are navigating to.
  bool AccruePrefetchBenefits(const GURL& referrer,
                              DnsHostInfo* navigation_info);

  // Instigate prefetch of any domains we predict will be needed after this
  // navigation.
  void NavigatingTo(const std::string& host_name);

  // Record details of a navigation so that we can preresolve the host name
  // ahead of time the next time the users navigates to the indicated host.
  void NonlinkNavigation(const GURL& referrer, DnsHostInfo* navigation_info);

  // Dump HTML table containing list of referrers for about:dns.
  void GetHtmlReferrerLists(std::string* output);

  // Dump the list of currently know referrer domains and related prefetchable
  // domains.
  void GetHtmlInfo(std::string* output);

  // Discard any referrer for which all the suggested host names are currently
  // annotated with no user latency reduction.  Also scale down (diminish) the
  // total benefit of those that did help, so that their reported contribution
  // wll go done by a factor of 2 each time we trim (moving the referrer closer
  // to being discarded at a future Trim).
  void TrimReferrers();

  // Construct a ListValue object that contains all the data in the referrers_
  // so that it can be persisted in a pref.
  void SerializeReferrers(ListValue* referral_list);

  // Process a ListValue that contains all the data from a previous reference
  // list, as constructed by SerializeReferrers(), and add all the identified
  // values into the current referrer list.
  void DeserializeReferrers(const ListValue& referral_list);

 private:
  FRIEND_TEST(DnsMasterTest, BenefitLookupTest);
  FRIEND_TEST(DnsMasterTest, ShutdownWhenResolutionIsPendingTest);
  FRIEND_TEST(DnsMasterTest, SingleLookupTest);
  FRIEND_TEST(DnsMasterTest, ConcurrentLookupTest);
  FRIEND_TEST(DnsMasterTest, MassiveConcurrentLookupTest);
  friend class WaitForResolutionHelper;  // For testing.

  class LookupRequest;

  // A map that is keyed with the hostnames that we've learned were the cause
  // of loading additional hostnames.  The list of additional hostnames in held
  // in a Referrer instance, which is found in this type.
  typedef std::map<std::string, Referrer> Referrers;

  // Only for testing. Returns true if hostname has been successfully resolved
  // (name found).
  bool WasFound(const std::string& hostname) {
    AutoLock auto_lock(lock_);
    return (results_.find(hostname) != results_.end()) &&
            results_[hostname].was_found();
  }

  // Only for testing. Return how long was the resolution
  // or DnsHostInfo::kNullDuration if it hasn't been resolved yet.
  base::TimeDelta GetResolutionDuration(const std::string& hostname) {
    AutoLock auto_lock(lock_);
    if (results_.find(hostname) == results_.end())
      return DnsHostInfo::kNullDuration;
    return results_[hostname].resolve_duration();
  }

  // Only for testing;
  size_t peak_pending_lookups() const { return peak_pending_lookups_; }

  // Access method for use by lookup request to pass resolution result.
  void OnLookupFinished(LookupRequest* request,
                        const std::string& hostname, bool found);

  // "PreLocked" means that the caller has already Acquired lock_ in the
  // following method names.
  // Queue hostname for resolution.  If queueing was done, return the pointer
  // to the queued instance, otherwise return NULL.
  DnsHostInfo* PreLockedResolve(const std::string& hostname,
                                DnsHostInfo::ResolutionMotivation motivation);

  // Take lookup requests from name_buffer_ and tell HostResolver
  // to look them up asynchronously, provided we don't exceed
  // concurrent resolution limit.
  void PreLockedScheduleLookups();

  // Synchronize access to variables listed below.
  Lock lock_;

  // name_buffer_ holds a list of names we need to look up.
  std::queue<std::string> name_buffer_;

  // results_ contains information for existing/prior prefetches.
  Results results_;

  // For each hostname that we might navigate to (that we've "learned about")
  // we have a Referrer list. Each Referrer list has all hostnames we need to
  // pre-resolve when there is a navigation to the orginial hostname.
  Referrers referrers_;

  std::set<LookupRequest*> pending_lookups_;

  // For testing, to verify that we don't exceed the limit.
  size_t peak_pending_lookups_;

  // When true, we don't make new lookup requests.
  bool shutdown_;

  // A list of successful events resulting from pre-fetching.
  DnsHostInfo::DnsInfoTable cache_hits_;
  // A map of hosts that were evicted from our cache (after we prefetched them)
  // and before the HTTP stack tried to look them up.
  Results cache_eviction_map_;

  DISALLOW_COPY_AND_ASSIGN(DnsMaster);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_MASTER_H_
