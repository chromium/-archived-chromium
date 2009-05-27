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

bool HighresTimer::perf_ratio_collected_ = false;
mach_timebase_info_data_t HighresTimer::perf_ratio_ = {0};

const uint64 NANOS_IN_MILLI = 1000000L;
const uint64 NANOS_IN_HALF_MILLI = 500000L;
const uint64 NANOS_IN_SECOND = 1000000000L;
const uint64 NANOS_IN_HALF_SECOND = 500000000L;

uint64 HighresTimer::GetElapsedMs() const {
  uint64 end_time = GetCurrentTicks();

  // Scale to ms and round to nearest ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  (void)GetTimerFrequency();
  return (static_cast<uint64>(end_time - start_ticks_) * perf_ratio_.numer +
          NANOS_IN_HALF_MILLI * perf_ratio_.denom) /
      (NANOS_IN_MILLI * perf_ratio_.denom);
}

uint64 HighresTimer::GetElapsedSec() const {
  uint64 end_time = GetCurrentTicks();

  // Scale to ms and round to nearest ms - rounding is important
  // because otherwise the truncation error may accumulate e.g. in sums.
  //
  (void)GetTimerFrequency();
  return (static_cast<uint64>(end_time - start_ticks_) * perf_ratio_.numer +
          NANOS_IN_HALF_SECOND * perf_ratio_.denom) /
      (NANOS_IN_SECOND * perf_ratio_.denom);
}

void HighresTimer::CollectPerfRatio() {
  mach_timebase_info(&perf_ratio_);
  perf_ratio_collected_ = true;
}
