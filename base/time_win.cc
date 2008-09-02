// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"

#pragma comment(lib, "winmm.lib")
#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/singleton.h"

namespace {

// From MSDN, FILETIME "Contains a 64-bit value representing the number of
// 100-nanosecond intervals since January 1, 1601 (UTC)."
int64 FileTimeToMicroseconds(const FILETIME& ft) {
  // Need to bit_cast to fix alignment, then divide by 10 to convert
  // 100-nanoseconds to milliseconds. This only works on little-endian
  // machines.
  return bit_cast<int64, FILETIME>(ft) / 10;
}

void MicrosecondsToFileTime(int64 us, FILETIME* ft) {
  DCHECK(us >= 0) << "Time is less than 0, negative values are not "
      "representable in FILETIME";

  // Multiply by 10 to convert milliseconds to 100-nanoseconds. Bit_cast will
  // handle alignment problems. This only works on little-endian machines.
  *ft = bit_cast<FILETIME, int64>(us * 10);
}

}  // namespace

// Time -----------------------------------------------------------------------

// The internal representation of Time uses FILETIME, whose epoch is 1601-01-01
// 00:00:00 UTC.  ((1970-1601)*365+89)*24*60*60*1000*1000, where 89 is the
// number of leap year days between 1601 and 1970: (1970-1601)/4 excluding
// 1700, 1800, and 1900.
// static
const int64 Time::kTimeTToMicrosecondsOffset = GG_INT64_C(11644473600000000);

// static
int64 Time::CurrentWallclockMicroseconds() {
  FILETIME ft;
  ::GetSystemTimeAsFileTime(&ft);
  return FileTimeToMicroseconds(ft);
}

// static
Time Time::FromFileTime(FILETIME ft) {
  return Time(FileTimeToMicroseconds(ft));
}

FILETIME Time::ToFileTime() const {
  FILETIME utc_ft;
  MicrosecondsToFileTime(us_, &utc_ft);
  return utc_ft;
}

// static
Time Time::FromExploded(bool is_local, const Exploded& exploded) {
  // Create the system struct representing our exploded time. It will either be
  // in local time or UTC.
  SYSTEMTIME st;
  st.wYear = exploded.year;
  st.wMonth = exploded.month;
  st.wDayOfWeek = exploded.day_of_week;
  st.wDay = exploded.day_of_month;
  st.wHour = exploded.hour;
  st.wMinute = exploded.minute;
  st.wSecond = exploded.second;
  st.wMilliseconds = exploded.millisecond;

  // Convert to FILETIME.
  FILETIME ft;
  if (!SystemTimeToFileTime(&st, &ft)) {
    NOTREACHED() << "Unable to convert time";
    return Time(0);
  }

  // Ensure that it's in UTC.
  if (is_local) {
    FILETIME utc_ft;
    LocalFileTimeToFileTime(&ft, &utc_ft);
    return Time(FileTimeToMicroseconds(utc_ft));
  }
  return Time(FileTimeToMicroseconds(ft));
}

void Time::Explode(bool is_local, Exploded* exploded) const {
  // FILETIME in UTC.
  FILETIME utc_ft;
  MicrosecondsToFileTime(us_, &utc_ft);

  // FILETIME in local time if necessary.
  BOOL success = TRUE;
  FILETIME ft;
  if (is_local)
    success = FileTimeToLocalFileTime(&utc_ft, &ft);
  else
    ft = utc_ft;

  // FILETIME in SYSTEMTIME (exploded).
  SYSTEMTIME st;
  if (!success || !FileTimeToSystemTime(&ft, &st)) {
    NOTREACHED() << "Unable to convert time, don't know why";
    ZeroMemory(exploded, sizeof(exploded));
    return;
  }

  exploded->year = st.wYear;
  exploded->month = st.wMonth;
  exploded->day_of_week = st.wDayOfWeek;
  exploded->day_of_month = st.wDay;
  exploded->hour = st.wHour;
  exploded->minute = st.wMinute;
  exploded->second = st.wSecond;
  exploded->millisecond = st.wMilliseconds;
}

// TimeTicks ------------------------------------------------------------------
TimeTicks::TickFunction TimeTicks::tick_function_ =
    reinterpret_cast<TickFunction>(&timeGetTime);

namespace {

// We use timeGetTime() to implement TimeTicks::Now().  This can be problematic
// because it returns the number of millisecond since Windows has started, which
// will roll over the 32-bit value every ~49 days.  We try to track rollover
// ourselves, which works if TimeTicks::Now() is called at least every 49 days.
class NowSingleton {
 public:
  NowSingleton()
      : rollover_(TimeDelta::FromMilliseconds(0)),
        last_seen_(TimeTicks::tick_function_()) { }

  TimeDelta Now() {
    AutoLock locked(lock_);
    // We should hold the lock while calling tick_function_ to make sure that
    // we keep our last_seen_ stay correctly in sync.
    DWORD now = TimeTicks::tick_function_();
    if (now < last_seen_)
      rollover_ += TimeDelta::FromMilliseconds(0x100000000I64);  // ~49.7 days.
    last_seen_ = now;
    return TimeDelta::FromMilliseconds(now) + rollover_;
  }

 private:
  Lock lock_;  // To protected last_seen_ and rollover_.
  TimeDelta rollover_;  // Accumulation of time lost do to rollover.
  DWORD last_seen_;  // The last timeGetTime value we saw, to detect rollover.

  DISALLOW_COPY_AND_ASSIGN(NowSingleton);
};

// Overview of time counters:
// (1) CPU cycle counter. (Retrieved via RDTSC)
// The CPU counter provides the highest resolution time stamp and is the least
// expensive to retrieve. However, the CPU counter is unreliable and should not
// be used in production. Its biggest issue is that it is per processor and it
// is not synchronized between processors. Also, on some computers, the counters
// will change frequency due to thermal and power changes, and stop in some
// states.
//
// (2) QueryPerformanceCounter (QPC). The QPC counter provides a high-
// resolution (100 nanoseconds) time stamp but is comparatively more expensive
// to retrieve. What QueryPerformanceCounter actually does is up to the HAL.
// (with some help from ACPI).
// According to http://blogs.msdn.com/oldnewthing/archive/2005/09/02/459952.aspx
// in the worst case, it gets the counter from the rollover interrupt on the
// programmable interrupt timer. In best cases, the HAL may conclude that the
// RDTSC counter runs at a constant frequency, then it uses that instead. On
// multiprocessor machines, it will try to verify the values returned from
// RDTSC on each processor are consistent with each other, and apply a handful
// of workarounds for known buggy hardware. In other words, QPC is supposed to
// give consistent result on a multiprocessor computer, but it is unreliable in
// reality due to bugs in BIOS or HAL on some, especially old computers.
// With recent updates on HAL and newer BIOS, QPC is getting more reliable but
// it should be used with caution.
//
// (3) System time. The system time provides a low-resolution (typically 10ms
// to 55 milliseconds) time stamp but is comparatively less expensive to
// retrieve and more reliable.
class UnreliableHighResNowSingleton {
 public:
  UnreliableHighResNowSingleton() : ticks_per_microsecond_(0) {
    LARGE_INTEGER ticks_per_sec = {0};
    if (!QueryPerformanceFrequency(&ticks_per_sec))
      return;  // Broken, we don't guarantee this function works.
    ticks_per_microsecond_ =
        ticks_per_sec.QuadPart / Time::kMicrosecondsPerSecond;
  }

  bool IsBroken() {
    return ticks_per_microsecond_ == 0;
  }

  TimeDelta Now() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return TimeDelta::FromMicroseconds(now.QuadPart / ticks_per_microsecond_);
  }

 private:
  // Cached clock frequency -> microseconds. This assumes that the clock
  // frequency is faster than one microsecond (which is 1MHz, should be OK).
  int64 ticks_per_microsecond_;  // 0 indicates QPF failed and we're broken.

  DISALLOW_COPY_AND_ASSIGN(UnreliableHighResNowSingleton);
};

}  // namespace

// static
TimeTicks TimeTicks::Now() {
  return TimeTicks() + Singleton<NowSingleton>::get()->Now();
}

// static
TimeTicks TimeTicks::UnreliableHighResNow() {
  UnreliableHighResNowSingleton* now =
      Singleton<UnreliableHighResNowSingleton>::get();

  if (now->IsBroken()) {
    NOTREACHED() << "QueryPerformanceCounter is broken.";
    return TimeTicks(0);
  }

  return TimeTicks() + now->Now();
}
