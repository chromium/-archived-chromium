// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A DnsMaster object is instantiated once in the browser
// process, and delivers DNS prefetch assignments (hostnames)
// to any of several DnsSlave objects.
// Most hostname lists are sent out by renderer processes, and
// involve lists of hostnames that *might* be used in the near
// future by the browsing user.  The goal of this class is to
// cause the underlying DNS structure to lookup a hostname before
// it is really needed, and hence reduce latency in the standard
// lookup paths.  Since some DNS lookups may take a LONG time, we
// use several DnsSlave threads to concurrently perform the
// lookups.

#ifndef CHROME_BROWSER_NET_DNS_MASTER_H_
#define CHROME_BROWSER_NET_DNS_MASTER_H_

#include <map>
#include <queue>
#include <string>

#include "base/condition_variable.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/common/net/dns.h"
#include "googleurl/src/url_canon.h"

namespace chrome_browser_net {

class DnsSlave;

typedef chrome_common_net::NameList NameList;
typedef std::map<std::string, DnsHostInfo> Results;

class DnsMaster {
 public:
  // The number of slave processes that will do DNS prefetching
  static const size_t kSlaveCountMax = 8;

  explicit DnsMaster(base::TimeDelta shutdown_wait_time);

  ~DnsMaster() {
    if (!shutdown_)
      ShutdownSlaves();  // Ensure we did our cleanup.
  }

  // ShutdownSlaves() gets all spawned threads to terminate, closes
  // their handles, and deletes their DnsSlave instances.
  // Return value of true means all operations succeeded.
  // Return value of false means that the threads wouldn't terminate,
  // and that resources may leak.  If this returns false, it is best
  // to NOT delete this DnsMaster, as slave threads may still call into
  // this object.
  bool ShutdownSlaves();

  // In some circumstances, for privacy reasons, all results should be
  // discarded.  This method gracefully handles that activity.
  // Destroy all our internal state, which shows what names we've looked up, and
  // how long each has taken, etc. etc.  We also destroy records of suggesses
  // (cache hits etc.).
  void DiscardAllResults();

  // Add hostname(s) to the queue for processing by slaves
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

  // For testing only...
  // Currently testing only provides a crude measure of success.
  bool WasFound(const std::string& hostname) {
    AutoLock auto_lock(lock_);
    return (results_.find(hostname) != results_.end()) &&
            results_[hostname].was_found();
  }

  // Accessor methods, used mostly for testing.
  // Both functions return DnsHostInfo::kNullDuration if name was not yet
  // processed enough.
  base::TimeDelta GetResolutionDuration(const std::string hostname) {
    AutoLock auto_lock(lock_);
    if (results_.find(hostname) == results_.end())
      return DnsHostInfo::kNullDuration;
    return results_[hostname].resolve_duration();
  }

  base::TimeDelta GetQueueDuration(const std::string hostname) {
    AutoLock auto_lock(lock_);
    if (results_.find(hostname) == results_.end())
      return DnsHostInfo::kNullDuration;
    return results_[hostname].queue_duration();
  }

  size_t running_slave_count() {
    AutoLock auto_lock(lock_);
    return running_slave_count_;
  }

  //----------------------------------------------------------------------------
  // Methods below this line should only be called by slave processes.

  // GetNextAssignment() gets the next hostname from queue for processing
  // It is not meant to be public, and should only be used by the slave.
  // GetNextAssignment() waits on a condition variable if there are no more
  // names in queue.
  // Return false if slave thread should terminate.
  // Return true if slave thread should process the value.
  bool GetNextAssignment(std::string* hostname);

  // Access methods for use by slave threads to callback with state updates.
  void SetFoundState(const std::string hostname);
  void SetNoSuchNameState(const std::string hostname);

  // Notification during ShutdownSlaves.
  void SetSlaveHasTerminated(int slave_index);

 private:
  // A map that is keyed with the hostnames that we've learned were the cause
  // of loading additional hostnames.  The list of additional hostnames in held
  // in a Referrer instance, which is found in this type.
  typedef std::map<std::string, Referrer> Referrers;

  // "PreLocked" means that the caller has already Acquired lock_ in the
  // following method names.
  // Queue hostname for resolution.  If queueing was done, return the pointer
  // to the queued instance, otherwise return NULL.
  DnsHostInfo* PreLockedResolve(const std::string& hostname,
                                DnsHostInfo::ResolutionMotivation motivation);
  bool PreLockedCreateNewSlaveIfNeeded();  // Lazy slave processes creation.

  // Number of slave processes started early (to help with startup prefetch).
  static const size_t kSlaveCountMin = 4;

  // Synchronize access to results_, referrers_, and slave control data.
  Lock lock_;

  // name_buffer_ holds a list of names we need to look up.
  std::queue<std::string> name_buffer_;

  // results_ contains information for existing/prior prefetches.
  Results results_;

  // For each hostname that we might navigate to (that we've "learned about")
  // we have a Referrer list. Each Referrer list has all hostnames we need to
  // pre-resolve when there is a navigation to the orginial hostname.
  Referrers referrers_;

  // Signaling slaves to process elements in the queue, or to terminate,
  // is done using ConditionVariables.
  ConditionVariable slaves_have_work_;

  size_t slave_count_;  // Count of slave processes started.
  size_t running_slave_count_;  // Count of slaves process still running.

  // The following arrays are only initialized as
  // slave_count_ grows (up to the indicated max).
  DWORD thread_ids_[kSlaveCountMax];
  HANDLE thread_handles_[kSlaveCountMax];
  DnsSlave* slaves_[kSlaveCountMax];

  // shutdown_ is set to tell the slaves to terminate.
  bool shutdown_;

  // The following is the maximum time the ShutdownSlaves method
  // will wait for all the slave processes to terminate.
  const base::TimeDelta kShutdownWaitTime_;

  // A list of successful events resulting from pre-fetching.
  DnsHostInfo::DnsInfoTable cache_hits_;
  // A map of hosts that were evicted from our cache (after we prefetched them)
  // and before the HTTP stack tried to look them up.
  Results cache_eviction_map_;

  DISALLOW_COPY_AND_ASSIGN(DnsMaster);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_MASTER_H_

