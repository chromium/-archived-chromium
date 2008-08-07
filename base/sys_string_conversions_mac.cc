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

#include "base/sys_strings.h"

#include <CoreFoundation/CoreFoundation.h>

namespace base {

namespace {

// Convert the supplied cfsring into the specified encoding, and return it as
// an STL string of the template type.  Returns an empty string on failure.
//
// Do not assert in this function since it is used by the asssertion code!
template<typename StringType>
static StringType CFStringToSTLStringWithEncodingT(CFStringRef cfstring,
                                                   CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0)
    return StringType();

  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring,
                                       whole_string,
                                       encoding,
                                       0,      // lossByte
                                       false,  // isExternalRepresentation
                                       NULL,   // buffer
                                       0,      // maxBufLen
                                       &out_size);
  DCHECK(converted != 0 && out_size != 0);
  if (converted == 0 || out_size == 0)
    return StringType();

  // out_size is the number of UInt8-sized units needed in the destination.
  // A buffer allocated as UInt8 units might not be properly aligned to
  // contain elements of StringType::value_type.  Use a container for the
  // proper value_type, and convert out_size by figuring the number of
  // value_type elements per UInt8.  Leave room for a NUL terminator.
  typename StringType::size_type elements =
      out_size * sizeof(UInt8) / sizeof(typename StringType::value_type) + 1;

  std::vector<typename StringType::value_type> out_buffer(elements);
  converted = CFStringGetBytes(cfstring,
                               whole_string,
                               encoding,
                               0,      // lossByte
                               false,  // isExternalRepresentation
                               reinterpret_cast<UInt8*>(&out_buffer[0]),
                               out_size,
                               NULL);  // usedBufLen
  if (converted == 0)
    return StringType();

  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0]);
}

// Given an STL string |in| with an encoding specified by |in_encoding|,
// convert it to |out_encoding| and return it as an STL string of the
// |OutStringType| template type.  Returns an empty string on failure.
//
// Do not assert in this function since it is used by the asssertion code!
template<typename OutStringType, typename InStringType>
static OutStringType STLStringToSTLStringWithEncodingsT(
    const InStringType& in,
    CFStringEncoding in_encoding,
    CFStringEncoding out_encoding) {
  typename InStringType::size_type in_length = in.length();
  if (in_length == 0)
    return OutStringType();

  scoped_cftyperef<CFStringRef> cfstring(
      CFStringCreateWithBytesNoCopy(NULL,
                                    reinterpret_cast<const UInt8*>(in.c_str()),
                                    in_length *
                                      sizeof(typename InStringType::value_type),
                                    in_encoding,
                                    false,
                                    kCFAllocatorNull));
  DCHECK(cfstring);
  if (!cfstring)
    return OutStringType();

  return CFStringToSTLStringWithEncodingT<OutStringType>(cfstring,
                                                         out_encoding);
}

// Specify the byte ordering explicitly, otherwise CFString will be confused
// when strings don't carry BOMs, as they typically won't.
static const CFStringEncoding kNarrowStringEncoding = kCFStringEncodingUTF8;
#ifdef __BIG_ENDIAN__
#if defined(__WCHAR_MAX__) && __WCHAR_MAX__ == 0xffff
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF16BE;
#else  // __WCHAR_MAX__
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF32BE;
#endif  // __WCHAR_MAX__
#else  // __BIG_ENDIAN__
#if defined(__WCHAR_MAX__) && __WCHAR_MAX__ == 0xffff
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF16LE;
#else  // __WCHAR_MAX__
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF32LE;
#endif  // __WCHAR_MAX__
#endif  // __BIG_ENDIAN__

}  // namespace

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToUTF8(const std::wstring& wide) {
  return STLStringToSTLStringWithEncodingsT<std::string>(
      wide, kWideStringEncoding, kNarrowStringEncoding);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysUTF8ToWide(const std::string& utf8) {
  return STLStringToSTLStringWithEncodingsT<std::wstring>(
      utf8, kNarrowStringEncoding, kWideStringEncoding);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  return WideToUTF8(wide);
}

std::wstring SysNativeMBToWide(const std::string& native_mb) {
  return UTF8ToWide(native_mb);
}

}  // namespace base
