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

#include "base/time_format.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "unicode/datefmt.h"

namespace {

std::wstring TimeFormat(const DateFormat* formatter,
                        const Time& time) {
  DCHECK(formatter);
  UnicodeString date_string;
  
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return UTF16ToWide(date_string.getTerminatedBuffer());
}

}

namespace base {

std::wstring TimeFormatTimeOfDay(const Time& time) {
  // We can omit the locale parameter because the default should match
  // Chrome's application locale.
  scoped_ptr<DateFormat> formatter(DateFormat::createTimeInstance(
      DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatShortDate(const Time& time) {
  scoped_ptr<DateFormat> formatter(DateFormat::createDateInstance(
      DateFormat::kMedium));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatShortDateNumeric(const Time& time) {
  scoped_ptr<DateFormat> formatter(DateFormat::createDateInstance(
      DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatShortDateAndTime(const Time& time) {
   scoped_ptr<DateFormat> formatter(DateFormat::createDateTimeInstance(
      DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatFriendlyDateAndTime(const Time& time) {
   scoped_ptr<DateFormat> formatter(DateFormat::createDateTimeInstance(
      DateFormat::kFull));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatFriendlyDate(const Time& time) {
  scoped_ptr<DateFormat> formatter(DateFormat::createDateInstance(
      DateFormat::kFull));
  return TimeFormat(formatter.get(), time);
}

}  // namespace base
