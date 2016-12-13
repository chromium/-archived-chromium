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


#ifndef O3D_STATSREPORT_COMMON_HIGHRES_TIMER_LINUX_H_
#define O3D_STATSREPORT_COMMON_HIGHRES_TIMER_LINUX_H_

#include "base/basictypes.h"

#include <sys/time.h>

const uint64 MICROS_IN_SECOND = 1000000L;

// A handy class for reliably measuring wall-clock time with decent resolution.
//
// We want to measure time with high resolution on Linux. What to do?
//
// RDTSC? Sure, but how do you convert it to wall clock time?
// clock_gettime? It's not in all Linuxes.
//
// Let's just use gettimeofday; it's good to the microsecond.
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
  // Captured start time
  uint64 start_ticks_;
};

inline HighresTimer::HighresTimer() {
  Start();
}

inline void HighresTimer::Start() {
  start_ticks_ = GetCurrentTicks();
}

inline uint64 HighresTimer::GetTimerFrequency() {
  // fixed; one "tick" is one microsecond

  return MICROS_IN_SECOND;
}

inline uint64 HighresTimer::GetCurrentTicks() {
  timeval tv;
  (void)gettimeofday(&tv, 0);

  return tv.tv_sec * MICROS_IN_SECOND + tv.tv_usec;
}

inline uint64 HighresTimer::GetElapsedTicks() const {
  return start_ticks_ - GetCurrentTicks();
}

#endif  // O3D_STATSREPORT_COMMON_HIGHRES_TIMER_LINUX_H_
