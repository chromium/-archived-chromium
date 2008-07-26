// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/time.h"
#include "base/string_util.h"
#include "base/third_party/nspr/prtime.h"

#include "base/logging.h"

namespace {

// Time between resampling the un-granular clock for this API.  60 seconds.
const int kMaxMillisecondsToAvoidDrift = 60 * Time::kMillisecondsPerSecond;

}  // namespace

// TimeDelta ------------------------------------------------------------------

// static
TimeDelta TimeDelta::FromDays(int64 days) {
  return TimeDelta(days * Time::kMicrosecondsPerDay);
}

// static
TimeDelta TimeDelta::FromHours(int64 hours) {
  return TimeDelta(hours * Time::kMicrosecondsPerHour);
}

// static
TimeDelta TimeDelta::FromMinutes(int64 minutes) {
  return TimeDelta(minutes * Time::kMicrosecondsPerMinute);
}

// static
TimeDelta TimeDelta::FromSeconds(int64 secs) {
  return TimeDelta(secs * Time::kMicrosecondsPerSecond);
}

// static
TimeDelta TimeDelta::FromMilliseconds(int64 ms) {
  return TimeDelta(ms * Time::kMicrosecondsPerMillisecond);
}

// static
TimeDelta TimeDelta::FromMicroseconds(int64 us) {
  return TimeDelta(us);
}

int TimeDelta::InDays() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerDay);
}

int TimeDelta::InHours() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerHour);
}

double TimeDelta::InSecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerSecond;
}

int64 TimeDelta::InSeconds() const {
  return delta_ / Time::kMicrosecondsPerSecond;
}

double TimeDelta::InMillisecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerMillisecond;
}

int64 TimeDelta::InMilliseconds() const {
  return delta_ / Time::kMicrosecondsPerMillisecond;
}

int64 TimeDelta::InMicroseconds() const {
  return delta_;
}

// Time -----------------------------------------------------------------------

int64 Time::initial_time_ = 0;
TimeTicks Time::initial_ticks_;

// static
void Time::InitializeClock()
{
    initial_ticks_ = TimeTicks::Now();
    initial_time_ = CurrentWallclockMicroseconds();
}

// static
Time Time::Now() {
  if (initial_time_ == 0)
    InitializeClock();

  // We implement time using the high-resolution timers so that we can get
  // timeouts which are smaller than 10-15ms.  If we just used
  // CurrentWallclockMicroseconds(), we'd have the less-granular timer.
  //
  // To make this work, we initialize the clock (initial_time) and the
  // counter (initial_ctr).  To compute the initial time, we can check
  // the number of ticks that have elapsed, and compute the delta.
  //
  // To avoid any drift, we periodically resync the counters to the system
  // clock.
  while(true) {
    TimeTicks ticks = TimeTicks::Now();

    // Calculate the time elapsed since we started our timer
    TimeDelta elapsed = ticks - initial_ticks_;

    // Check if enough time has elapsed that we need to resync the clock.
    if (elapsed.InMilliseconds() > kMaxMillisecondsToAvoidDrift) {
      InitializeClock();
      continue;
    }

    return elapsed + initial_time_;
  }
}

// static
Time Time::FromTimeT(time_t tt) {
  if (tt == 0)
    return Time();  // Preserve 0 so we can tell it doesn't exist.
  return (tt * kMicrosecondsPerSecond) + kTimeTToMicrosecondsOffset;
}

time_t Time::ToTimeT() const {
  if (us_ == 0)
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  return (us_ - kTimeTToMicrosecondsOffset) / kMicrosecondsPerSecond;
}

double Time::ToDoubleT() const {
  if (us_ == 0)
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  return (static_cast<double>(us_ - kTimeTToMicrosecondsOffset) /
          static_cast<double>(kMicrosecondsPerSecond));
}

Time Time::LocalMidnight() const {
  Exploded exploded;
  LocalExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  return FromLocalExploded(exploded);
}

// static
bool Time::FromString(const wchar_t* time_string, Time* parsed_time) {
  DCHECK((time_string != NULL) && (parsed_time != NULL));
  std::string ascii_time_string = WideToUTF8(time_string);
  if (ascii_time_string.length() == 0)
    return false;
  PRTime result_time = 0;
  PRStatus result = PR_ParseTimeString(ascii_time_string.c_str(), PR_FALSE,
                                       &result_time);
  if (PR_SUCCESS != result)
    return false;
  result_time += kTimeTToMicrosecondsOffset;
  *parsed_time = Time(result_time);
  return true;
}
