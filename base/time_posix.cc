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

#ifdef OS_MACOSX
#include <mach/mach_time.h>
#endif
#include <sys/time.h>
#include <time.h>

#include "base/basictypes.h"
#include "base/logging.h"

// The Time routines in this file use standard POSIX routines, or almost-
// standard routines in the case of timegm.  We need to use a Mach-specific
// function for TimeTicks::Now() on Mac OS X.

// Time -----------------------------------------------------------------------

// The internal representation of Time uses time_t directly, so there is no
// offset.  The epoch is 1970-01-01 00:00:00 UTC.
// static
const int64 Time::kTimeTToMicrosecondsOffset = GG_INT64_C(0);

// static
int64 Time::CurrentWallclockMicroseconds() {
  struct timeval tv;
  struct timezone tz = { 0, 0 };  // UTC
  if (gettimeofday(&tv, &tz) != 0) {
    DCHECK(0) << "Could not determine time of day";
  }
  // Combine seconds and microseconds in a 64-bit field containing microseconds
  // since the epoch.  That's enough for nearly 600 centuries.
  return tv.tv_sec * kMicrosecondsPerSecond + tv.tv_usec;
}

// static
Time Time::FromExploded(bool is_local, const Exploded& exploded) {
  struct tm timestruct;
  timestruct.tm_sec    = exploded.second;
  timestruct.tm_min    = exploded.minute;
  timestruct.tm_hour   = exploded.hour;
  timestruct.tm_mday   = exploded.day_of_month;
  timestruct.tm_mon    = exploded.month - 1;
  timestruct.tm_year   = exploded.year - 1900;
  timestruct.tm_wday   = exploded.day_of_week;  // mktime/timegm ignore this
  timestruct.tm_yday   = 0;     // mktime/timegm ignore this
  timestruct.tm_isdst  = -1;    // attempt to figure it out
  timestruct.tm_gmtoff = 0;     // not a POSIX field, so mktime/timegm ignore
  timestruct.tm_zone   = NULL;  // not a POSIX field, so mktime/timegm ignore

  time_t seconds;
  if (is_local)
    seconds = mktime(&timestruct);
  else
    seconds = timegm(&timestruct);
  DCHECK(seconds >= 0) << "mktime/timegm could not convert from exploded";

  uint64 milliseconds = seconds * kMillisecondsPerSecond + exploded.millisecond;
  return Time(milliseconds * kMicrosecondsPerMillisecond);
}

void Time::Explode(bool is_local, Exploded* exploded) const {
  // Time stores times with microsecond resolution, but Exploded only carries
  // millisecond resolution, so begin by being lossy.
  uint64 milliseconds = us_ / kMicrosecondsPerMillisecond;
  time_t seconds = milliseconds / kMillisecondsPerSecond;

  struct tm timestruct;
  if (is_local)
    localtime_r(&seconds, &timestruct);
  else
    gmtime_r(&seconds, &timestruct);

  exploded->year         = timestruct.tm_year + 1900;
  exploded->month        = timestruct.tm_mon + 1;
  exploded->day_of_week  = timestruct.tm_wday;
  exploded->day_of_month = timestruct.tm_mday;
  exploded->hour         = timestruct.tm_hour;
  exploded->minute       = timestruct.tm_min;
  exploded->second       = timestruct.tm_sec;
  exploded->millisecond  = milliseconds % kMillisecondsPerSecond;
}

// TimeTicks ------------------------------------------------------------------

// static
TimeTicks TimeTicks::Now() {
  uint64_t absolute_micro;

#if defined(OS_MACOSX)

  static bool has_timebase_info = false;
  static mach_timebase_info_data_t timebase_info = {1, 1};
  if (!has_timebase_info) {
    has_timebase_info = mach_timebase_info(&timebase_info) == KERN_SUCCESS;
  }
  DCHECK(has_timebase_info) << "Could not determine system tick rate";

  // mach_absolute_time is it when it comes to ticks on the Mac.  Other calls
  // with less precision (such as TickCount) just call through to
  // mach_absolute_time.

  // timebase_info converts absolute time tick units into nanoseconds.  Convert
  // to microseconds up front to stave off overflows.
  absolute_micro = mach_absolute_time() / Time::kNanosecondsPerMicrosecond *
                   timebase_info.numer / timebase_info.denom;

  // Don't bother with the rollover handling that the Windows version does.
  // With numer and denom = 1 (the expected case), the 64-bit absolute time
  // reported in nanoseconds is enough to last nearly 585 years.

#elif defined(OS_POSIX) && \
      defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    NOTREACHED() << "clock_gettime(CLOCK_MONOTONIC) failed.";
    return TimeTicks();
  }

  absolute_micro =
      (static_cast<int64>(ts.tv_sec) * Time::kMicrosecondsPerSecond) +
      (static_cast<int64>(ts.tv_nsec) / Time::kNanosecondsPerMicrosecond);

#else  // _POSIX_MONOTONIC_CLOCK
#error No usable tick clock function on this platform.
#endif  // _POSIX_MONOTONIC_CLOCK

  return TimeTicks(absolute_micro);
}

// static
TimeTicks TimeTicks::UnreliableHighResNow() {
  return Now();
}
