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

#include <windows.h>
#include <string>
#include "unicode/numfmt.h"
#include "base/logging.h"

// See WideToUTF8.
static std::string WideToMultiByte(const std::wstring& wide, UINT code_page) {
  int wide_length = static_cast<int>(wide.length());
  if (wide_length == 0)
    return std::string();

  // compute the length of the buffer we'll need
  int charcount = WideCharToMultiByte(code_page, 0, wide.data(), wide_length,
                                      NULL, 0, NULL, NULL);
  if (charcount == 0)
    return std::string();

  // convert
  std::string mb;
  WideCharToMultiByte(code_page, 0, wide.data(), wide_length,
                      WriteInto(&mb, charcount + 1), charcount, NULL, NULL);

  return mb;
}

// Converts the given 8-bit string into a wide string, using the given
// code page. The code page identifier is one accepted by MultiByteToWideChar()
//
// Danger: do not assert in this function, as it is used by the assertion code.
// Doing so will cause an infinite loop.
static std::wstring MultiByteToWide(const std::string& mb, UINT code_page) {
  if (mb.length() == 0)
    return std::wstring();

  // compute the length of the buffer
  int charcount = MultiByteToWideChar(code_page, 0, mb.c_str(), -1, NULL, 0);
  if (charcount == 0)
    return std::wstring();

  // convert
  std::wstring wide;
  MultiByteToWideChar(code_page, 0, mb.c_str(), -1,
                      WriteInto(&wide, charcount), charcount);

  return wide;
}

// Wide <--> native multibyte
std::string WideToNativeMB(const std::wstring& wide) {
  return WideToMultiByte(wide, CP_ACP);
}

std::wstring NativeMBToWide(const std::string& native_mb) {
  return MultiByteToWide(native_mb, CP_ACP);
}

NumberFormat* NumberFormatSingleton() {
  static NumberFormat* number_format = NULL;
  if (!number_format) {
    // Make sure we are thread-safe.
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat* new_number_format = NumberFormat::createInstance(status);
    if (U_FAILURE(status)) {
      NOTREACHED();
    }
    if (InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID*>(&number_format), new_number_format, NULL)) {
      // The old value was non-NULL, so no replacement was done. Another
      // thread did the initialization out from under us.
      if (new_number_format)
        delete new_number_format;
    }
  }
  return number_format;
}

int64 StringToInt64(const std::string& value) {
  return _atoi64(value.c_str());
}

int64 StringToInt64(const std::wstring& value) {
  return _wtoi64(value.c_str());
}
