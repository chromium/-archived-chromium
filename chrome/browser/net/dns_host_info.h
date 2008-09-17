// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A DnsHostInfo object is used to store status of a Dns lookup of a specific
// hostname.
// It includes progress, from placement in the DnsMaster's queue, to assignment
// to a slave, to resolution by the (blocking) DNS service as either FOUND or
// NO_SUCH_NAME.

#ifndef CHROME_BROWSER_NET_DNS_HOST_INFO_H__
#define CHROME_BROWSER_NET_DNS_HOST_INFO_H__

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/time.h"

namespace chrome_browser_net {

// Use command line switch to enable detailed logging.
void EnableDnsDetailedLog(bool enable);

enum DnsBenefit {
  PREFETCH_NO_BENEFIT,  // Prefetch never hit the network. Name was pre-cached.
  PREFETCH_CACHE_EVICTION,  // Prefetch used network, but so did HTTP stack.
  PREFETCH_NAME_NONEXISTANT,  // Valuable prefetch of "name not found" was used.
  PREFETCH_NAME_FOUND,  // Valuable prefetch was used.
  PREFETCH_OBLIVIOUS  // No prefetch attempt was even made.
};

class DnsHostInfo {
 public:
  enum DnsProcessingState {
      // When processed by our prefetching system, there are 4 states:
      PENDING,       // Constructor has completed.
      QUEUED,        // In prefetch queue but not yet assigned to a slave.
      ASSIGNED,      // Currently being processed by a slave.
      ASSIGNED_BUT_MARKED,  // Needs to be deleted as soon as slave is done.
      FOUND,         // DNS prefetch search completed.
      NO_SUCH_NAME,  // DNS prefetch search completed.
      // When processed by the HTTP network stack, there are 3 states:
      STARTED,               // Resolution has begun.
      FINISHED,              // Resolution has completed.
      FINISHED_UNRESOLVED};  // No resolution found.
  static const TimeDelta kMaxNonNetworkDnsLookupDuration;
  // The number of OS cache entries we can guarantee(?) before cache eviction
  // might likely take place.
  static const int kMaxGuaranteedCacheSize = 50;

  typedef std::vector<DnsHostInfo> DnsInfoTable;

  static const TimeDelta kNullDuration;

  // DnsHostInfo are usually made by the default constructor during
  // initializing of the DnsMaster's map (of info for Hostnames).
  DnsHostInfo()
      : state_(PENDING),
        resolve_duration_(kNullDuration),
        queue_duration_(kNullDuration),
        benefits_remaining_(),
        sequence_number_(0) {
  }

  ~DnsHostInfo() {}

  // NeedDnsUpdate decides, based on our internal info,
  // if it would be valuable to attempt to update (prefectch)
  // DNS data for hostname.  This decision is based
  // on how recently we've done DNS prefetching for hostname.
  bool NeedsDnsUpdate(const std::string& hostname);

  static void set_cache_expiration(TimeDelta time);

  // The prefetching lifecycle.
  void SetQueuedState();
  void SetAssignedState();
  void SetPendingDeleteState();
  void SetFoundState();
  void SetNoSuchNameState();
  // The actual browsing resolution lifecycle.
  void SetStartedState();
  void SetFinishedState(bool was_resolved);

  void SetHostname(const std::string& hostname) {
    if (hostname != hostname_) {
      DCHECK(hostname_.size() == 0);  // Not yet initialized.
      hostname_ = hostname;
    }
  }

  bool was_found() const { return FOUND == state_; }
  bool was_nonexistant() const { return NO_SUCH_NAME == state_; }
  bool is_assigned() const {
    return ASSIGNED == state_ || ASSIGNED_BUT_MARKED == state_;
  }
  bool is_marked_to_delete() const { return ASSIGNED_BUT_MARKED == state_; }
  const std::string hostname() const { return hostname_; }

  bool HasHostname(const std::string& hostname) const {
    return (hostname == hostname_);
  }

  TimeDelta resolve_duration() const { return resolve_duration_;}
  TimeDelta queue_duration() const { return queue_duration_;}
  TimeDelta benefits_remaining() const { return benefits_remaining_; }

  DnsBenefit AcruePrefetchBenefits(DnsHostInfo* later_host_info);

  void DLogResultsStats(const char* message) const;

  static void GetHtmlTable(const DnsInfoTable host_infos,
                           const char* description,
                           const bool brief,
                           std::string* output);

 private:
  // The next declaration is non-const to facilitate testing.
  static TimeDelta kCacheExpirationDuration;

  DnsProcessingState state_;
  std::string hostname_;  // Hostname for this info.

  TimeTicks time_;  // When was last state changed (usually lookup completed).
  TimeDelta resolve_duration_;  // Time needed for DNS to resolve.
  TimeDelta queue_duration_;  // Time spent in queue.
  TimeDelta benefits_remaining_;  // Unused potential benefits of a prefetch.

  int sequence_number_;  // Used to calculate potential of cache eviction.
  static int sequence_counter;  // Used to allocate sequence_number_'s.

  TimeDelta GetDuration() {
    TimeTicks old_time = time_;
    time_ = TimeTicks::Now();
    return time_ - old_time;
  }

  // IsStillCached() guesses if the DNS cache still has IP data.
  bool IsStillCached() const;

  // We put these objects into a std::map, and hence we
  // need some "evil" constructors.
  // DISALLOW_EVIL_CONSTRUCTORS(DnsHostInfo);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_HOST_INFO_H__

