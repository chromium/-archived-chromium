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


// This file contains declarations for the PerformanceTimer class, which
// is used to measure o3d performance using the highest resolution
// timers available on a given platform.  This should be a cross-platform
// header:  actual implementation of the timer is platform-specific, and
// should reside in the platform-specific directory (eg.,
// win/performance_timer.cc).

#ifndef O3D_IMPORT_CROSS_PERFORMANCE_TIMER_H_
#define O3D_IMPORT_CROSS_PERFORMANCE_TIMER_H_

#include <build/build_config.h>
#ifdef OS_MACOSX
#include <Carbon/Carbon.h>
#endif
#include <string>
#include "base/basictypes.h"

namespace o3d {

#ifdef OS_MACOSX
typedef AbsoluteTime PerformanceTimeStamp;
#else
typedef uint64 PerformanceTimeStamp;
#endif

// PerformanceTimer is designed to accurately track wallclock time
// for performance profiling.  Between Start() and Stop(), a
// "stopwatch" accumulates elapsed time into accum_time_.  Print() will
// print a short string to the logger containing the name of the timer,
// and its elapsed time.

class PerformanceTimer {
 public:
  explicit PerformanceTimer(const char *name);

  // Starts the timer.
  void Start();

  // Stops the timer.
  void Stop();

  // Prints the name and currently elapsed timer value to the logger.
  void Print();

  // Stops and prints the timer.
  void StopAndPrint();

  // Returns the timer's elapsed time, as of the last Stop(), in seconds.
  double GetElapsedTime();

  // Returns the name of the timer.
  const char* name() { return name_.c_str(); }
 private:

  // Name of the timer.
  std::string name_;

  // Time the timer was last started, in internal units.
  PerformanceTimeStamp start_time_;

  // Accumulated elapsed time, in internal units.  Computed only on Stop().
  PerformanceTimeStamp accum_time_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceTimer);
};
} // end namespace o3d

#endif  // O3D_IMPORT_CROSS_PERFORMANCE_TIMER_H_
