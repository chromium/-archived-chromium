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

#include <string.h>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "unicode/ucnv.h"
#include "unicode/numfmt.h"
#include "unicode/ustring.h"

// Codepage <-> Wide -----------------------------------------------------------

// Convert a unicode string into the specified codepage_name.  If the codepage
// isn't found, return false.
bool WideToCodepage(const std::wstring& wide,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::string* encoded) {
  encoded->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  const UChar* uchar_src;
  int uchar_len;
#ifdef U_WCHAR_IS_UTF16
  uchar_src = wide.c_str();
  uchar_len = static_cast<int>(wide.length());
#else  // U_WCHAR_IS_UTF16
  // When wchar_t is wider than UChar (16 bits), transform |wide| into a
  // UChar* string.  Size the UChar* buffer to be large enough to hold twice
  // as many UTF-16 code points as there are UCS-4 characters, in case each
  // character translates to a UTF-16 surrogate pair, and leave room for a NUL
  // terminator.
  std::vector<UChar> wide_uchar(wide.length() * 2 + 1);
  u_strFromWCS(&wide_uchar[0], wide_uchar.size(), &uchar_len,
               wide.c_str(), wide.length(), &status);
  uchar_src = &wide_uchar[0];
  DCHECK(U_SUCCESS(status)) << "failed to convert wstring to UChar*";
#endif  // U_WCHAR_IS_UTF16

  int encoded_max_length = UCNV_GET_MAX_BYTES_FOR_STRING(uchar_len,
    ucnv_getMaxCharSize(converter));
  encoded->resize(encoded_max_length);

  // Setup our error handler.
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_STOP, 0,
                            NULL, NULL, &status);
      break;
    case OnStringUtilConversionError::SKIP:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_SKIP, 0,
                            NULL, NULL, &status);
      break;
    default:
      NOTREACHED();
  }

  // ucnv_fromUChars returns size not including terminating null
  int actual_size = ucnv_fromUChars(converter, &(*encoded)[0],
    encoded_max_length, uchar_src, uchar_len, &status);
  encoded->resize(actual_size);
  ucnv_close(converter);
  if (U_SUCCESS(status))
    return true;
  encoded->clear();  // Make sure the output is empty on error.
  return false;
}

// Converts a string of the given codepage into unicode.
// If the codepage isn't found, return false.
bool CodepageToWide(const std::string& encoded,
                    const char* codepage_name,
                    OnStringUtilConversionError::Type on_error,
                    std::wstring* wide) {
  wide->clear();

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(codepage_name, &status);
  if (!U_SUCCESS(status))
    return false;

  // The worst case is all the input characters are non-BMP (32-bit) ones.
  size_t uchar_max_length = encoded.length() * 2 + 1;

  UChar* uchar_dst;
#ifdef U_WCHAR_IS_UTF16
  uchar_dst = WriteInto(wide, uchar_max_length);
#else
  // When wchar_t is wider than UChar (16 bits), convert into a temporary
  // UChar* buffer.
  std::vector<UChar> wide_uchar(uchar_max_length);
  uchar_dst = &wide_uchar[0];
#endif  // U_WCHAR_IS_UTF16

  // Setup our error handler.
  switch (on_error) {
    case OnStringUtilConversionError::FAIL:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_STOP, 0,
                          NULL, NULL, &status);
      break;
    case OnStringUtilConversionError::SKIP:
      ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_SKIP, 0,
                          NULL, NULL, &status);
      break;
    default:
      NOTREACHED();
  }

  int actual_size = ucnv_toUChars(converter,
                                  uchar_dst,
                                  static_cast<int>(uchar_max_length),
                                  encoded.data(),
                                  static_cast<int>(encoded.length()),
                                  &status);
  ucnv_close(converter);
  if (!U_SUCCESS(status)) {
    wide->clear();  // Make sure the output is empty on error.
    return false;
  }

#ifndef U_WCHAR_IS_UTF16
  // When wchar_t is wider than UChar (16 bits), it's not possible to wind up
  // with any more wchar_t elements than UChar elements.  ucnv_toUChars
  // returns the number of UChar elements not including the NUL terminator, so
  // leave extra room for that.
  u_strToWCS(WriteInto(wide, actual_size + 1), actual_size + 1, &actual_size,
             uchar_dst, actual_size, &status);
  DCHECK(U_SUCCESS(status)) << "failed to convert UChar* to wstring";
#endif  // U_WCHAR_IS_UTF16

  wide->resize(actual_size);
  return true;
}

// Number formatting -----------------------------------------------------------

// TODO: http://b/id=1092584 Come up with a portable pthread_once, and use
// that to keep a singleton instead of putting it in the platform-dependent
// file.
NumberFormat* NumberFormatSingleton();

std::wstring FormatNumber(int64 number) {
  NumberFormat* number_format = NumberFormatSingleton();
  if (!number_format) {
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  UnicodeString ustr;
  number_format->format(number, ustr);

#ifdef U_WCHAR_IS_UTF16
  return std::wstring(ustr.getBuffer(),
                      static_cast<std::wstring::size_type>(ustr.length()));
#else  // U_WCHAR_IS_UTF16
  wchar_t buffer[64];  // A int64 is less than 20 chars long,  so 64 chars
                       // leaves plenty of room for formating stuff.
  int length = 0;
  UErrorCode error = U_ZERO_ERROR;
  u_strToWCS(buffer, 64, &length, ustr.getBuffer(), ustr.length() , &error);
  if (U_FAILURE(error)) {
    NOTREACHED();
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  return std::wstring(buffer, static_cast<std::wstring::size_type>(length));
#endif  // U_WCHAR_IS_UTF16
}
