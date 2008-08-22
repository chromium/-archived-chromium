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

#include "base/sys_string_conversions.h"

#include <vector>

#include "base/scoped_cftyperef.h"
#include "base/string_piece.h"

namespace base {

namespace {

// Convert the supplied CFString into the specified encoding, and return it as
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
template<typename InStringType, typename OutStringType>
static OutStringType STLStringToSTLStringWithEncodingsT(
    const InStringType& in,
    CFStringEncoding in_encoding,
    CFStringEncoding out_encoding) {
  typename InStringType::size_type in_length = in.length();
  if (in_length == 0)
    return OutStringType();

  scoped_cftyperef<CFStringRef> cfstring(
      CFStringCreateWithBytesNoCopy(NULL,
                                    reinterpret_cast<const UInt8*>(in.data()),
                                    in_length *
                                      sizeof(typename InStringType::value_type),
                                    in_encoding,
                                    false,
                                    kCFAllocatorNull));
  if (!cfstring)
    return OutStringType();

  return CFStringToSTLStringWithEncodingT<OutStringType>(cfstring,
                                                         out_encoding);
}

// Given an STL string |in| with an encoding specified by |in_encoding|,
// return it as a CFStringRef.  Returns NULL on failure.
template<typename StringType>
static CFStringRef STLStringToCFStringWithEncodingsT(
    const StringType& in,
    CFStringEncoding in_encoding) {
  typename StringType::size_type in_length = in.length();
  if (in_length == 0)
    return CFSTR("");

  return CFStringCreateWithBytes(kCFAllocatorDefault,
                                 reinterpret_cast<const UInt8*>(in.data()),
                                 in_length *
                                   sizeof(typename StringType::value_type),
                                 in_encoding,
                                 false);
}

// Specify the byte ordering explicitly, otherwise CFString will be confused
// when strings don't carry BOMs, as they typically won't.
static const CFStringEncoding kNarrowStringEncoding = kCFStringEncodingUTF8;
#ifdef __BIG_ENDIAN__
#if defined(WCHAR_T_IS_UTF16)
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF16BE;
#elif defined(WCHAR_T_IS_UTF32)
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF32BE;
#endif  // WCHAR_T_IS_UTF32
#elif defined(__LITTLE_ENDIAN__)
#if defined(WCHAR_T_IS_UTF16)
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF16LE;
#elif defined(WCHAR_T_IS_UTF32)
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF32LE;
#endif  // WCHAR_T_IS_UTF32
#endif  // __LITTLE_ENDIAN__

}  // namespace

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToUTF8(const std::wstring& wide) {
  return STLStringToSTLStringWithEncodingsT<std::wstring, std::string>(
      wide, kWideStringEncoding, kNarrowStringEncoding);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysUTF8ToWide(const StringPiece& utf8) {
  return STLStringToSTLStringWithEncodingsT<StringPiece, std::wstring>(
      utf8, kNarrowStringEncoding, kWideStringEncoding);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  return SysWideToUTF8(wide);
}

std::wstring SysNativeMBToWide(const StringPiece& native_mb) {
  return SysUTF8ToWide(native_mb);
}

CFStringRef SysUTF8ToCFStringRef(const std::string& utf8) {
  return STLStringToCFStringWithEncodingsT(utf8, kNarrowStringEncoding);
}

CFStringRef SysWideToCFStringRef(const std::wstring& wide) {
  return STLStringToCFStringWithEncodingsT(wide, kWideStringEncoding);
}

std::string SysCFStringRefToUTF8(CFStringRef ref) {
  return CFStringToSTLStringWithEncodingT<std::string>(ref,
                                                       kNarrowStringEncoding);
}

std::wstring SysCFStringRefToWide(CFStringRef ref) {
  return CFStringToSTLStringWithEncodingT<std::wstring>(ref,
                                                        kWideStringEncoding);
}

}  // namespace base
