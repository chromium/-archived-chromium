// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See header file for description of class

#include "chrome/browser/net/dns_host_info.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/histogram.h"
#include "base/logging.h"
#include "base/string_util.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace chrome_browser_net {

static bool detailed_logging_enabled = false;

// Use command line switch to enable detailed logging.
void EnableDnsDetailedLog(bool enable) {
  detailed_logging_enabled = enable;
}

// static
int DnsHostInfo::sequence_counter = 1;


bool DnsHostInfo::NeedsDnsUpdate(const std::string& hostname) {
  DCHECK(hostname == hostname_);
  switch (state_) {
    case PENDING:  // Just now created info.
      return true;

    case QUEUED:  // In queue.
    case ASSIGNED:  // It's being resolved.
    case ASSIGNED_BUT_MARKED:  // It's being resolved.
      return false;  // We're already working on it

    case NO_SUCH_NAME:  // Lookup failed.
    case FOUND:  // Lookup succeeded.
      return !IsStillCached();  // See if DNS cache expired.

    default:
      DCHECK(false);
      return false;
  }
}

const TimeDelta DnsHostInfo::kNullDuration(TimeDelta::FromMilliseconds(-1));

TimeDelta DnsHostInfo::kCacheExpirationDuration(TimeDelta::FromMinutes(5));

const TimeDelta DnsHostInfo::kMaxNonNetworkDnsLookupDuration(
    TimeDelta::FromMilliseconds(15));

void DnsHostInfo::set_cache_expiration(TimeDelta time) {
  kCacheExpirationDuration = time;
}

void DnsHostInfo::SetQueuedState(ResolutionMotivation motivation) {
  DCHECK(PENDING == state_ || FOUND == state_ || NO_SUCH_NAME == state_);
  state_ = QUEUED;
  queue_duration_ = resolve_duration_ = kNullDuration;
  SetMotivation(motivation);
  GetDuration();  // Set time_
  DLogResultsStats("DNS Prefetch in queue");
}

void DnsHostInfo::SetAssignedState() {
  DCHECK(QUEUED == state_);
  state_ = ASSIGNED;
  queue_duration_ = GetDuration();
  DLogResultsStats("DNS Prefetch assigned");
  UMA_HISTOGRAM_TIMES(L"DNS.PrefetchQueue", queue_duration_);
}

void DnsHostInfo::SetPendingDeleteState() {
  DCHECK(ASSIGNED == state_  || ASSIGNED_BUT_MARKED == state_);
  state_ = ASSIGNED_BUT_MARKED;
}

void DnsHostInfo::SetFoundState() {
  DCHECK(ASSIGNED == state_);
  state_ = FOUND;
  resolve_duration_ = GetDuration();
  if (kMaxNonNetworkDnsLookupDuration <= resolve_duration_) {
    UMA_HISTOGRAM_LONG_TIMES(L"DNS.PrefetchFoundNameL", resolve_duration_);
    // Record potential beneficial time, and maybe we'll get a cache hit.
    // We keep the maximum, as the warming we did earlier may still be
    // helping with a cache upstream in DNS resolution.
    benefits_remaining_ = std::max(resolve_duration_, benefits_remaining_);
  }
  sequence_number_ = sequence_counter++;
  DLogResultsStats("DNS PrefetchFound");
}

void DnsHostInfo::SetNoSuchNameState() {
  DCHECK(ASSIGNED == state_);
  state_ = NO_SUCH_NAME;
  resolve_duration_ = GetDuration();
  if (kMaxNonNetworkDnsLookupDuration <= resolve_duration_) {
    DHISTOGRAM_TIMES(L"DNS.PrefetchNotFoundName", resolve_duration_);
    // Record potential beneficial time, and maybe we'll get a cache hit.
    benefits_remaining_ = std::max(resolve_duration_, benefits_remaining_);
  }
  sequence_number_ = sequence_counter++;
  DLogResultsStats("DNS PrefetchNotFound");
}

void DnsHostInfo::SetStartedState() {
  DCHECK(PENDING == state_);
  state_ = STARTED;
  queue_duration_ = resolve_duration_ = TimeDelta();  // 0ms.
  SetMotivation(NO_PREFETCH_MOTIVATION);
  GetDuration();  // Set time.
}

void DnsHostInfo::SetFinishedState(bool was_resolved) {
  DCHECK(STARTED == state_);
  state_ = was_resolved ? FINISHED : FINISHED_UNRESOLVED;
  resolve_duration_ = GetDuration();
  // TODO(jar): Sequence number should be incremented in prefetched HostInfo.
  DLogResultsStats("DNS HTTP Finished");
}

void DnsHostInfo::SetHostname(const std::string& hostname) {
  if (hostname != hostname_) {
    DCHECK(hostname_.size() == 0);  // Not yet initialized.
    hostname_ = hostname;
  }
}

// IsStillCached() guesses if the DNS cache still has IP data,
// or at least remembers results about "not finding host."
bool DnsHostInfo::IsStillCached() const {
  DCHECK(FOUND == state_ || NO_SUCH_NAME == state_);

  // Default MS OS does not cache failures. Hence we could return false almost
  // all the time for that case.  However, we'd never try again to prefetch
  // the value if we returned false that way.  Hence we'll just let the lookup
  // time out the same way as FOUND case.

  if (sequence_counter - sequence_number_ > kMaxGuaranteedCacheSize)
    return false;

  TimeDelta time_since_resolution = TimeTicks::Now() - time_;

  if (FOUND == state_ && resolve_duration_ < kMaxNonNetworkDnsLookupDuration) {
    // Since cache was warm (no apparent network activity during resolution),
    // we assume it was "really" found (via network activity) twice as long
    // ago as when we got our FOUND result.
    time_since_resolution *= 2;
  }

  return time_since_resolution < kCacheExpirationDuration;
}

// Compare the actual navigation DNS latency found in navigation_info, to the
// previously prefetched info.
DnsBenefit DnsHostInfo::AccruePrefetchBenefits(DnsHostInfo* navigation_info) {
  DCHECK(FINISHED == navigation_info->state_ ||
         FINISHED_UNRESOLVED == navigation_info->state_);
  DCHECK(0 == navigation_info->hostname_.compare(hostname_.data()));

  if ((0 == benefits_remaining_.InMilliseconds()) ||
      (FOUND != state_ && NO_SUCH_NAME != state_)) {
    if (FINISHED == navigation_info->state_)
      UMA_HISTOGRAM_LONG_TIMES(L"DNS.IndependentNavigation",
                               navigation_info->resolve_duration_);
    else
      UMA_HISTOGRAM_LONG_TIMES(L"DNS.IndependentFailedNavigation",
                               navigation_info->resolve_duration_);
    return PREFETCH_NO_BENEFIT;
  }

  TimeDelta benefit = benefits_remaining_ - navigation_info->resolve_duration_;
  navigation_info->benefits_remaining_ = benefits_remaining_;
  benefits_remaining_ = TimeDelta();  // We used up all our benefits here.

  navigation_info->motivation_ = motivation_;
  if (LEARNED_REFERAL_MOTIVATED == motivation_ ||
      STATIC_REFERAL_MOTIVATED == motivation_)
    navigation_info->referring_hostname_ = referring_hostname_;

  if (navigation_info->resolve_duration_ > kMaxNonNetworkDnsLookupDuration) {
    // Our precache effort didn't help since HTTP stack hit the network.
    UMA_HISTOGRAM_LONG_TIMES(L"DNS.PrefetchCacheEvictionL", resolve_duration_);
    DLogResultsStats("DNS PrefetchCacheEviction");
    return PREFETCH_CACHE_EVICTION;
  }

  if (NO_SUCH_NAME == state_) {
    UMA_HISTOGRAM_LONG_TIMES(L"DNS.PrefetchNegativeHitL", benefit);
    DLogResultsStats("DNS PrefetchNegativeHit");
    return PREFETCH_NAME_NONEXISTANT;
  }

  DCHECK_EQ(FOUND, state_);
  if (LEARNED_REFERAL_MOTIVATED == motivation_ ||
      STATIC_REFERAL_MOTIVATED == motivation_) {
    UMA_HISTOGRAM_TIMES(L"DNS.PrefetchReferredPositiveHit", benefit);
    DLogResultsStats("DNS PrefetchReferredPositiveHit");
  } else {
    UMA_HISTOGRAM_LONG_TIMES(L"DNS.PrefetchPositiveHitL", benefit);
    DLogResultsStats("DNS PrefetchPositiveHit");
  }
  return PREFETCH_NAME_FOUND;
}

void DnsHostInfo::DLogResultsStats(const char* message) const {
  if (!detailed_logging_enabled)
    return;
  DLOG(INFO) << "\t" << message <<  "\tq="
      << queue_duration().InMilliseconds() << "ms,\tr="
      << resolve_duration().InMilliseconds() << "ms\tp="
      << benefits_remaining_.InMilliseconds() << "ms\tseq="
      << sequence_number_
      << "\t" << hostname_;
}

//------------------------------------------------------------------------------
// This last section supports HTML output, such as seen in about:dns.
//------------------------------------------------------------------------------

// Preclude any possibility of Java Script or markup in the text, by only
// allowing alphanumerics, ".", and whitespace.
static std::string RemoveJs(const std::string& text) {
  std::string output(text);
  size_t length = output.length();
  for (size_t i = 0; i < length; i++) {
    char next = output[i];
    if (isalnum(next) || isspace(next) || '.' == next || '-' == next)
      continue;
    output[i] = '?';
  }
  return output;
}

class MinMaxAverage {
 public:
  MinMaxAverage()
    : sum_(0), square_sum_(0), count_(0),
      minimum_(kint64max), maximum_(kint64min) {
  }

  // Return values for use in printf formatted as "%d"
  int sample(int64 value) {
    sum_ += value;
    square_sum_ += value * value;
    count_++;
    minimum_ = std::min(minimum_, value);
    maximum_ = std::max(maximum_, value);
    return static_cast<int>(value);
  }
  int minimum() const { return static_cast<int>(minimum_);    }
  int maximum() const { return static_cast<int>(maximum_);    }
  int average() const { return static_cast<int>(sum_/count_); }
  int     sum() const { return static_cast<int>(sum_);        }

  int standard_deviation() const {
    double average = static_cast<float>(sum_) / count_;
    double variance = static_cast<float>(square_sum_)/count_
                      - average * average;
    return static_cast<int>(floor(sqrt(variance) + .5));
  }

 private:
  int64 sum_;
  int64 square_sum_;
  int count_;
  int64 minimum_;
  int64 maximum_;

  // DISALLOW_COPY_AND_ASSIGN(MinMaxAverage);
};

static std::string HoursMinutesSeconds(int seconds) {
  std::string result;
  int print_seconds = seconds % 60;
  int minutes = seconds / 60;
  int print_minutes = minutes % 60;
  int print_hours = minutes/60;
  if (print_hours)
    StringAppendF(&result, "%.2d:",  print_hours);
  if (print_hours || print_minutes)
    StringAppendF(&result, "%2.2d:",  print_minutes);
  StringAppendF(&result, "%2.2d",  print_seconds);
  return result;
}

// static
void DnsHostInfo::GetHtmlTable(const DnsInfoTable host_infos,
                               const char* description,
                               const bool brief,
                               std::string* output) {
  if (0 == host_infos.size())
    return;
  output->append(description);
  StringAppendF(output, "%d %s", host_infos.size(),
                (1 == host_infos.size()) ? "hostname" : "hostnames");

  if (brief) {
    output->append("<br><br>");
    return;
  }

  const char* row_format = "<tr align=right><td>%s</td>"
                           "<td>%d</td><td>%d</td><td>%s</td><td>%s</td></tr>";

  output->append("<br><table border=1>");
  StringAppendF(output,
      "<tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th></tr>",
      "Host name", "Applicable Prefetch<br>Time (ms)",
      "Recent Resolution<br>Time(ms)", "How long ago<br>(HH:MM:SS)",
      "Motivation");

  // Print bulk of table, and gather stats at same time.
  MinMaxAverage queue, resolve, preresolve, when;
  TimeTicks current_time = TimeTicks::Now();
  for (DnsInfoTable::const_iterator it(host_infos.begin());
       it != host_infos.end(); it++) {
    queue.sample((it->queue_duration_.InMilliseconds()));
    StringAppendF(output, row_format,
                  RemoveJs(it->hostname_).c_str(),
                  preresolve.sample((it->benefits_remaining_.InMilliseconds())),
                  resolve.sample((it->resolve_duration_.InMilliseconds())),
                  HoursMinutesSeconds(when.sample(
                      (current_time - it->time_).InSeconds())).c_str(),
                  it->GetAsciiMotivation().c_str());
  }
  // Write min, max, and average summary lines.
  if (host_infos.size() > 2) {
    output->append("<B>");
    StringAppendF(output, row_format,
                  "<b>---minimum---</b>",
                  preresolve.minimum(), resolve.minimum(),
                  HoursMinutesSeconds(when.minimum()).c_str(), "");
    StringAppendF(output, row_format,
                  "<b>---average---</b>",
                  preresolve.average(), resolve.average(),
                  HoursMinutesSeconds(when.average()).c_str(), "");
    StringAppendF(output, row_format,
                  "<b>standard deviation</b>",
                  preresolve.standard_deviation(),
                  resolve.standard_deviation(), "n/a", "");
    StringAppendF(output, row_format,
                  "<b>---maximum---</b>",
                  preresolve.maximum(), resolve.maximum(),
                  HoursMinutesSeconds(when.maximum()).c_str(), "");
    StringAppendF(output, row_format,
                  "<b>-----SUM-----</b>",
                  preresolve.sum(), resolve.sum(), "n/a", "");
  }
  output->append("</table>");

#ifdef DEBUG
  StringAppendF(output,
                "Prefetch Queue Durations: min=%d, avg=%d, max=%d<br><br>",
                queue.minimum(), queue.average(), queue.maximum());
#endif

  output->append("<br>");
}

void DnsHostInfo::SetMotivation(ResolutionMotivation motivation) {
  motivation_ = motivation;
  if (motivation < LINKED_MAX_MOTIVATED)
    was_linked_ = true;
}

std::string DnsHostInfo::GetAsciiMotivation() const {
  switch (motivation_) {
    case MOUSE_OVER_MOTIVATED:
      return "[mouse-over]";

    case PAGE_SCAN_MOTIVATED:
      return "[page scan]";

    case OMNIBOX_MOTIVATED:
      return "[omnibox]";

    case STARTUP_LIST_MOTIVATED:
      return "[startup list]";

    case NO_PREFETCH_MOTIVATION:
      return "n/a";

    case STATIC_REFERAL_MOTIVATED:
      return RemoveJs(referring_hostname_) + "*";

    case LEARNED_REFERAL_MOTIVATED:
      return RemoveJs(referring_hostname_);

    default:
      return "";
  }
}

}  // namespace chrome_browser_net

