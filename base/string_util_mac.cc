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

#include "base/string_util.h"

#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <string>
#include <vector>
#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "unicode/numfmt.h"

// Can't use strlcpy/wcslcpy, because they always returns the length of src,
// making it impossible to detect overflow.  Because the reimplementation is
// too large to inline, StrNCpy and WcsNCpy are in this file, but since they
// don't make any non-inlined calls, there's no penalty relative to the libc
// routines.
template<typename CharType>
static inline bool StrNCpyT(CharType* dst, const CharType* src,
                            size_t dst_size, size_t src_size) {
  // The initial value of count has room for a NUL terminator.
  size_t count = std::min(dst_size, src_size + 1);
  if (count == 0)
    return false;

  // Copy up to (count - 1) bytes, or until reaching a NUL terminator
  while (--count != 0) {
    if ((*dst++ = *src++) == '\0')
      break;
  }

  // If the break never occurred, append a NUL terminator
  if (count == 0) {
    *dst = '\0';

    // If the string was truncated, return false
    if (*src != '\0')
      return false;
  }

  return true;
}

bool StrNCpy(char* dst, const char* src,
             size_t dst_size, size_t src_size) {
  return StrNCpyT(dst, src, dst_size, src_size);
}

bool WcsNCpy(wchar_t* dst, const wchar_t* src,
             size_t dst_size, size_t src_size) {
  return StrNCpyT(dst, src, dst_size, src_size);
}

static NumberFormat* number_format_singleton = NULL;
static CFDateFormatterRef date_formatter = NULL;
static CFDateFormatterRef time_formatter = NULL;

static void DoInitializeStatics() {
  UErrorCode status = U_ZERO_ERROR;
  number_format_singleton = NumberFormat::createInstance(status);
  DCHECK(U_SUCCESS(status));

  scoped_cftyperef<CFLocaleRef> user_locale(CFLocaleCopyCurrent());
  date_formatter = CFDateFormatterCreate(NULL,
                                         user_locale,
                                         kCFDateFormatterShortStyle,   // date
                                         NULL);                        // time
  DCHECK(date_formatter);
  time_formatter = CFDateFormatterCreate(NULL,
                                         user_locale,
                                         NULL,                         // date
                                         kCFDateFormatterShortStyle);  // time
  DCHECK(time_formatter);
}

static void InitializeStatics() {
  static pthread_once_t pthread_once_initialized = PTHREAD_ONCE_INIT;
  pthread_once(&pthread_once_initialized, DoInitializeStatics);
}

// Technically, the native multibyte encoding would be the encoding returned
// by CFStringGetSystemEncoding or GetApplicationTextEncoding, but I can't
// imagine anyone needing or using that from these APIs, so just treat UTF-8
// as though it were the native multibyte encoding.
std::string WideToNativeMB(const std::wstring& wide) {
  return WideToUTF8(wide);
}

std::wstring NativeMBToWide(const std::string& native_mb) {
  return UTF8ToWide(native_mb);
}

NumberFormat* NumberFormatSingleton() {
  InitializeStatics();
  return number_format_singleton;
}

int64 StringToInt64(const std::string& value) {
  return atoll(value.c_str());
}

int64 StringToInt64(const std::wstring& value) {
  return wcstoll(value.c_str(), NULL, 10);
}
