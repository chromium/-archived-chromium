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


// This file contains the definitions of Timer related classes

#include <build/build_config.h>
#ifdef OS_LINUX
#include <sys/time.h>
#include <time.h>
#endif
#include "core/cross/precompile.h"
#include "core/cross/timer.h"

namespace o3d {

#ifdef OS_LINUX
static uint64_t GetCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec*1000000ULL + tv.tv_usec;
}
#endif

ElapsedTimeTimer::ElapsedTimeTimer() {
#if defined(OS_WIN)
  // Get the frequency of the windows performance clock.
  QueryPerformanceFrequency(&windows_timer_frequency_);

  // Initialize the last_time_ value with the current time.
  QueryPerformanceCounter(&last_time_);
#endif

#if defined(OS_MACOSX)
  last_time_ = UpTime();
#endif
#if defined(OS_LINUX)
  last_time_ = GetCurrentTime();
#endif
}

float ElapsedTimeTimer::GetElapsedTimeHelper(bool reset) {
  float elapsedTime = 0.0f;
  // Get current performance timer value.
  TimeStamp current_time;

#ifdef OS_WIN
  QueryPerformanceCounter(&current_time);

  // Compute elapsed time since the last call.
  elapsedTime = static_cast<float>(
    static_cast<double>(current_time.QuadPart - last_time_.QuadPart) /
    static_cast<double>(windows_timer_frequency_.QuadPart));
#endif

#ifdef OS_MACOSX
  current_time = UpTime();
  AbsoluteTime elapsed_ticks = SubAbsoluteFromAbsolute(current_time,
                                                       last_time_);
  Nanoseconds elapsedInNanos = AbsoluteToNanoseconds(elapsed_ticks);
  uint64 ns64 = UnsignedWideToUInt64(elapsedInNanos);
  double elapsedInSeconds = static_cast<double>(ns64) * 0.000000001;
  elapsedTime = elapsedInSeconds;
#endif

#ifdef OS_LINUX
  current_time = GetCurrentTime();
  elapsedTime = static_cast<float>((current_time - last_time_) * 1.e-6);
#endif

  if (reset) {
    // Save the current time for the next time we ask.
    last_time_ = current_time;
  }

  return elapsedTime;
}

float ElapsedTimeTimer::GetElapsedTimeWithoutClearing() {
  return GetElapsedTimeHelper(false);
}

float ElapsedTimeTimer::GetElapsedTimeAndReset() {
  return GetElapsedTimeHelper(true);
}

}  // namespace o3d
