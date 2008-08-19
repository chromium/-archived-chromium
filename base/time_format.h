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

// Basic time formatting methods.  These methods use the current locale
// formatting for displaying the time.

#ifndef BASE_TIME_FORMAT_H_
#define BASE_TIME_FORMAT_H_

#include <string>

class Time;

namespace base {

// Returns the time of day, e.g., "3:07 PM".
std::wstring TimeFormatTimeOfDay(const Time& time);

// Returns a shortened date, e.g. "Nov 7, 2007"
std::wstring TimeFormatShortDate(const Time& time);

// Returns a numeric date such as 12/13/52.
std::wstring TimeFormatShortDateNumeric(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008 2:44:30 PM".
std::wstring TimeFormatShortDateAndTime(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008 2:44:30 PM".
std::wstring TimeFormatFriendlyDateAndTime(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008".
std::wstring TimeFormatFriendlyDate(const Time& time);

}  // namespace base

#endif  // BASE_TIME_FORMAT_H_
