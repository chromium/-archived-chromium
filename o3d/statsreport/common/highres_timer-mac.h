/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_STATSREPORT_COMMON_HIGHRES_TIMER_MAC_H_
#define O3D_STATSREPORT_COMMON_HIGHRES_TIMER_MAC_H_

#include "base/basictypes.h"
#include <mach/mach.h>
#include <mach/mach_time.h>

// A handy class for reliably measuring wall-clock time with decent resolution.
class HighresTimer {
 public:
  // Captures the current start time
  HighresTimer();

  // Captures the current tick, can be used to reset a timer for reuse.
  void Start();

  // Returns the elapsed ticks with full resolution
  uint64 GetElapsedTicks() const;

  // Returns the elapsed time in milliseconds, rounded to the nearest
  // millisecond.
  uint64 GetElapsedMs() const;

  // Returns the elapsed time in seconds, rounded to the nearest second.
  uint64 GetElapsedSec() const;

  uint64 start_ticks() const { return start_ticks_; }

  // Returns timer frequency from cache, should be less
  // overhead than ::QueryPerformanceFrequency
  static uint64 GetTimerFrequency();
  // Returns current ticks
  static uint64 GetCurrentTicks();

 private:
  static void CollectPerfRatio();

  // Captured start time
  uint64 start_ticks_;

  // Captured performance counter frequency
  static bool perf_ratio_collected_;
  static mach_timebase_info_data_t perf_ratio_;
};

inline HighresTimer::HighresTimer() {
  Start();
}

inline void HighresTimer::Start() {
  start_ticks_ = GetCurrentTicks();
}

inline uint64 HighresTimer::GetTimerFrequency() {
  if (!perf_ratio_collected_)
    CollectPerfRatio();
  // we're losing precision by doing the division here, but this value is only
  // used to estimate tick time by the unit tests, so we're ok
  return static_cast<uint64>(perf_ratio_.denom) * 1000000000ULL
      / perf_ratio_.numer;
}

inline uint64 HighresTimer::GetCurrentTicks() {
  return mach_absolute_time();
}

inline uint64 HighresTimer::GetElapsedTicks() const {
  return start_ticks_ - GetCurrentTicks();
}

#endif  // O3D_STATSREPORT_COMMON_HIGHRES_TIMER_MAC_H_
