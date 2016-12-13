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


// This file contains the declarations of Timer related classes

#ifndef O3D_CORE_CROSS_TIMER_H_
#define O3D_CORE_CROSS_TIMER_H_

#include <build/build_config.h>
#ifdef OS_MACOSX
#include <Carbon/Carbon.h>
#endif

#include "core/cross/types.h"

namespace o3d {

class ElapsedTimeTimer {
 public:
  ElapsedTimeTimer();
  // Gets the elapsed time in seconds since the last time the timer was reset,
  // then reset the stored time to restart the interval.
  float GetElapsedTimeAndReset();
  // Gets the elapsed time in seconds since the last time the timer was reset,
  // but doesn't reset the stored time.  Use this to keep track of cumulative
  // time rather than each interval.
  float GetElapsedTimeWithoutClearing();

 private:
  float GetElapsedTimeHelper(bool reset);

#ifdef OS_MACOSX
  typedef AbsoluteTime TimeStamp;
#endif

#ifdef OS_LINUX
  typedef uint64_t TimeStamp;
#endif

#ifdef OS_WIN
  typedef LARGE_INTEGER TimeStamp;
#endif

#ifdef OS_WIN
  // The frequency of the windows performance counter in ticks per second.
  TimeStamp windows_timer_frequency_;
#endif

  // The value of the tick count from the windows performance counter from the
  // last time GetElapsedTime was called.
  TimeStamp last_time_;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_TIMER_H_
