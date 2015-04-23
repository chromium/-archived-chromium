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


#include "statsreport/common/highres_timer.h"

bool HighresTimer::perf_freq_collected_ = false;
ULONGLONG HighresTimer::perf_freq_ = 0;

ULONGLONG HighresTimer::GetElapsedMs() const {
  ULONGLONG end_time = GetCurrentTicks();

  // Scale to ms and round to nearerst ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  // Given infinite resolution, this expression could be written as:
  //  trunc((end - start (units:freq*sec))/freq (units:sec) *
  //                1000 (unit:ms) + 1/2 (unit:ms))
  ULONGLONG freq = GetTimerFrequency();
  return ((end_time - start_ticks_) * 1000L + freq / 2) / freq;
}

ULONGLONG HighresTimer::GetElapsedSec() const {
  ULONGLONG end_time = GetCurrentTicks();

  // Scale to ms and round to nearerst ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  // Given infinite resolution, this expression could be written as:
  //  trunc((end - start (units:freq*sec))/freq (unit:sec) + 1/2 (unit:sec))
  ULONGLONG freq = GetTimerFrequency();
  return ((end_time - start_ticks_) + freq / 2) / freq;
}

void HighresTimer::CollectPerfFreq() {
  LARGE_INTEGER freq;

  // Note that this is racy.  It's OK, however, because even
  // concurrent executions of this are idempotent.
  if (::QueryPerformanceFrequency(&freq)) {
    perf_freq_ = freq.QuadPart;
    perf_freq_collected_ = true;
  }
}
