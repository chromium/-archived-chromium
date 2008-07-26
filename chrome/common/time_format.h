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

#ifndef CHROME_COMMON_TIME_FORMAT_H__
#define CHROME_COMMON_TIME_FORMAT_H__

// This file defines methods to format time values as strings.

#include <string>

#include "unicode/smpdtfmt.h"

class Time;
class TimeDelta;

class TimeFormat {
 public:
  // Returns a localized string of approximate time remaining. The conditions
  // are simpler than PastTime since this is used for in-progress operations
  // and users have different expectations of units.
  // Ex: "3 mins left", "2 days left".
  static std::wstring TimeRemaining(const TimeDelta& delta);

  // Same as TimeRemaining without the "left".
  static std::wstring TimeRemainingShort(const TimeDelta& delta);

  // For displaying a relative time in the past.  This method returns either
  // "Today", "Yesterday", or an empty string if it's older than that.
  //
  // TODO(brettw): This should be able to handle days in the future like
  //    "Tomorrow".
  // TODO(tc): This should be able to do things like "Last week".  This
  //    requires handling singluar/plural for all languages.
  //
  // The second parameter is optional, it is midnight of "Now" for relative day
  // computations: Time::Now().LocalMidnight()
  // If NULL, the current day's midnight will be retrieved, which can be
  // slow. If many items are being processed, it is best to get the current
  // time once at the beginning and pass it for each computation.
  static std::wstring FriendlyDay(const Time& time,
                                  const Time* optional_midnight_today);

  // Returns the time of day, e.g., "3:07 PM".
  static std::wstring TimeOfDay(const Time& time);

  // Returns a shortened date, e.g. "Nov 7, 2007"
  static std::wstring ShortDate(const Time& time);

  // Returns a numeric date such as 12/13/52.
  static std::wstring ShortDateNumeric(const Time& time);

  // Formats a time in a friendly sentence format, e.g.
  // "Monday, March 6, 2008 2:44:30 PM".
  static std::wstring FriendlyDateAndTime(const Time& time);

  // Formats a time in a friendly sentence format, e.g.
  // "Monday, March 6, 2008".
  static std::wstring FriendlyDate(const Time& time);

  // Returns a time format used in a cookie expires attribute, e.g.
  // "Wed, 25-Apr-2007 21:02:13 GMT"
  // Its only legal time zone is GMT, and it can be parsed by
  // CookieMonster::ParseCookieTime().
  static std::wstring CookieExpires(const Time& time);
};

#endif  // CHROME_COMMON_TIME_FORMAT_H__
