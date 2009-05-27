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


#ifndef O3D_STATSREPORT_COMMON_HIGHRES_TIMER_WIN32_H_
#define O3D_STATSREPORT_COMMON_HIGHRES_TIMER_WIN32_H_

#include <windows.h>

// A handy class for reliably measuring wall-clock time with decent resolution,
// even on multi-processor machines and on laptops (where RDTSC potentially
// returns different results on different processors and/or the RDTSC timer
// clocks at different rates depending on the power state of the CPU,
// respectively).
class HighresTimer {
 public:
  // Captures the current start time
  HighresTimer();
  virtual ~HighresTimer() {}

  // Captures the current tick, can be used to reset a timer for reuse.
  void Start();

  // Returns the elapsed ticks with full resolution
  ULONGLONG GetElapsedTicks() const;

  // Returns the elapsed time in milliseconds, rounded to the nearest
  // millisecond.
  virtual ULONGLONG GetElapsedMs() const;

  // Returns the elapsed time in seconds, rounded to the nearest second.
  virtual ULONGLONG GetElapsedSec() const;

  ULONGLONG start_ticks() const { return start_ticks_; }

  // Returns timer frequency from cache, should be less
  // overhead than ::QueryPerformanceFrequency
  static ULONGLONG GetTimerFrequency();
  // Returns current ticks
  static ULONGLONG GetCurrentTicks();

 private:
  static void CollectPerfFreq();

  // Captured start time
  ULONGLONG start_ticks_;

  // Captured performance counter frequency
  static bool perf_freq_collected_;
  static ULONGLONG perf_freq_;
};

inline HighresTimer::HighresTimer() {
  Start();
}

inline void HighresTimer::Start() {
  start_ticks_ = GetCurrentTicks();
}

inline ULONGLONG HighresTimer::GetTimerFrequency() {
  if (!perf_freq_collected_)
    CollectPerfFreq();
  return perf_freq_;
}

inline ULONGLONG HighresTimer::GetCurrentTicks() {
  LARGE_INTEGER ticks;
  ::QueryPerformanceCounter(&ticks);
  return ticks.QuadPart;
}

inline ULONGLONG HighresTimer::GetElapsedTicks() const {
  return start_ticks_ - GetCurrentTicks();
}

#endif  // O3D_STATSREPORT_COMMON_HIGHRES_TIMER_WIN32_H_
