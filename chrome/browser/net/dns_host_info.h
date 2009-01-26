// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A DnsHostInfo object is used to store status of a Dns lookup of a specific
// hostname.
// It includes progress, from placement in the DnsMaster's queue, to assignment
// to a slave, to resolution by the (blocking) DNS service as either FOUND or
// NO_SUCH_NAME.

#ifndef CHROME_BROWSER_NET_DNS_HOST_INFO_H_
#define CHROME_BROWSER_NET_DNS_HOST_INFO_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

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
  // Reasons for a domain to be resolved.
  enum ResolutionMotivation {
    MOUSE_OVER_MOTIVATED,  // Mouse-over link induced resolution.
    PAGE_SCAN_MOTIVATED,   // Scan of rendered page induced resolution.
    UNIT_TEST_MOTIVATED,
    LINKED_MAX_MOTIVATED,    // enum demarkation above motivation from links.
    OMNIBOX_MOTIVATED,       // Omni-box suggested resolving this.
    STARTUP_LIST_MOTIVATED,  // Startup list caused this resolution.

    NO_PREFETCH_MOTIVATION,  // Browser navigation info (not prefetch related).

    // The following involve predictive prefetching, triggered by a navigation.
    // The referring_hostname_ is also set when these are used.
    // TODO(jar): Support STATIC_REFERAL_MOTIVATED API and integration.
    STATIC_REFERAL_MOTIVATED,  // External database suggested this resolution.
    LEARNED_REFERAL_MOTIVATED,  // Prior navigation taught us this resolution.
  };

  enum DnsProcessingState {
      // When processed by our prefetching system, the states are:
      PENDING,       // Constructor has completed.
      QUEUED,        // In prefetch queue but not yet assigned to a slave.
      ASSIGNED,      // Currently being processed by a slave.
      ASSIGNED_BUT_MARKED,  // Needs to be deleted as soon as slave is done.
      FOUND,         // DNS prefetch search completed.
      NO_SUCH_NAME,  // DNS prefetch search completed.
      // When processed by the network stack during navigation, the states are:
      STARTED,               // Resolution has begun for a navigation.
      FINISHED,              // Resolution has completed for a navigation.
      FINISHED_UNRESOLVED};  // No resolution found, so navigation will fail.
  static const base::TimeDelta kMaxNonNetworkDnsLookupDuration;
  // The number of OS cache entries we can guarantee(?) before cache eviction
  // might likely take place.
  static const int kMaxGuaranteedCacheSize = 50;

  typedef std::vector<DnsHostInfo> DnsInfoTable;

  static const base::TimeDelta kNullDuration;

  // DnsHostInfo are usually made by the default constructor during
  // initializing of the DnsMaster's map (of info for Hostnames).
  DnsHostInfo()
      : state_(PENDING),
        resolve_duration_(kNullDuration),
        queue_duration_(kNullDuration),
        benefits_remaining_(),
        sequence_number_(0),
        was_linked_(false) {
  }

  ~DnsHostInfo() {}

  // NeedDnsUpdate decides, based on our internal info,
  // if it would be valuable to attempt to update (prefectch)
  // DNS data for hostname.  This decision is based
  // on how recently we've done DNS prefetching for hostname.
  bool NeedsDnsUpdate(const std::string& hostname);

  static void set_cache_expiration(base::TimeDelta time);

  // The prefetching lifecycle.
  void SetQueuedState(ResolutionMotivation motivation);
  void SetAssignedState();
  void SetPendingDeleteState();
  void SetFoundState();
  void SetNoSuchNameState();
  // The actual browsing resolution lifecycle.
  void SetStartedState();
  void SetFinishedState(bool was_resolved);

  // Finish initialization. Must only be called once.
  void SetHostname(const std::string& hostname);

  bool was_linked() const { return was_linked_; }

  std::string referring_hostname() const { return referring_hostname_; }
  void SetReferringHostname(const std::string& hostname) {
    referring_hostname_ = hostname;
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

  base::TimeDelta resolve_duration() const { return resolve_duration_;}
  base::TimeDelta queue_duration() const { return queue_duration_;}
  base::TimeDelta benefits_remaining() const { return benefits_remaining_; }

  DnsBenefit AccruePrefetchBenefits(DnsHostInfo* navigation_info);

  void DLogResultsStats(const char* message) const;

  static void GetHtmlTable(const DnsInfoTable host_infos,
                           const char* description,
                           const bool brief,
                           std::string* output);

 private:
  base::TimeDelta GetDuration() {
    base::TimeTicks old_time = time_;
    time_ = base::TimeTicks::Now();
    return time_ - old_time;
  }

  // IsStillCached() guesses if the DNS cache still has IP data.
  bool IsStillCached() const;

  // Record why we created, or have updated (reqested pre-resolution) of this
  // instance.
  void SetMotivation(ResolutionMotivation motivation);

  // Helper function for about:dns printing.
  std::string GetAsciiMotivation() const;

  // The next declaration is non-const to facilitate testing.
  static base::TimeDelta kCacheExpirationDuration;

  DnsProcessingState state_;
  std::string hostname_;  // Hostname for this info.

  // When was last state changed (usually lookup completed).
  base::TimeTicks time_;
  // Time needed for DNS to resolve.
  base::TimeDelta resolve_duration_;
  // Time spent in queue.
  base::TimeDelta queue_duration_;
  // Unused potential benefits of a prefetch.
  base::TimeDelta benefits_remaining_;

  int sequence_number_;  // Used to calculate potential of cache eviction.
  static int sequence_counter;  // Used to allocate sequence_number_'s.

  // Motivation for creation of this instance.
  ResolutionMotivation motivation_;

  // Record if the motivation for prefetching was ever a page-link-scan.
  bool was_linked_;

  // If this instance holds data about a navigation, we store the referrer.
  // If this instance hold data about a prefetch, and the prefetch was
  // instigated by a referrer, we store it here (for use in about:dns).
  std::string referring_hostname_;

  // We put these objects into a std::map, and hence we
  // need some "evil" constructors.
  // DISALLOW_COPY_AND_ASSIGN(DnsHostInfo);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_HOST_INFO_H_
