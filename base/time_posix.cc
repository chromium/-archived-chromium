// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
Time Time::Now() {
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

  static mach_timebase_info_data_t timebase_info;
  if (timebase_info.denom == 0) {
    // Zero-initialization of statics guarantees that denom will be 0 before
    // calling mach_timebase_info.  mach_timebase_info will never set denom to
    // 0 as that would be invalid, so the zero-check can be used to determine
    // whether mach_timebase_info has already been called.  This is
    // recommended by Apple's QA1398.
    kern_return_t kr = mach_timebase_info(&timebase_info);
    DCHECK(kr == KERN_SUCCESS);
  }

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
TimeTicks TimeTicks::HighResNow() {
  return Now();
}
