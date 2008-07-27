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

#include "config.h"
#include <string>

#include "webkit/glue/glue_util.h"
#include "base/string_util.h"

#pragma warning(push, 0)
#include "CString.h"
#include "DeprecatedString.h"
#include "PlatformString.h"
#pragma warning(pop)

#include "KURL.h"

namespace webkit_glue {

// String conversions ----------------------------------------------------------

std::string CStringToStdString(const WebCore::CString& str) {
  const char* chars = str.data();
  return std::string(chars ? chars : "", str.length());
}

WebCore::CString StdStringToCString(const std::string& str) {
  return WebCore::CString(str.data(), str.size());
}

std::wstring StringToStdWString(const WebCore::String& str) {
  const UChar* chars = str.characters();
  return std::wstring(chars ? chars : L"", str.length());
}

WebCore::String StdWStringToString(const std::wstring& str) {
  return WebCore::String(str.data(), static_cast<unsigned>(str.length()));
}

WebCore::String StdStringToString(const std::string& str) {
  return WebCore::String(str.data(), static_cast<unsigned>(str.length()));
}

WebCore::DeprecatedString StdWStringToDeprecatedString(
    const std::wstring& str) {
  return WebCore::DeprecatedString(
      reinterpret_cast<const WebCore::DeprecatedChar*>(str.c_str()),
      static_cast<int>(str.size()));
}

std::wstring DeprecatedStringToStdWString(
    const WebCore::DeprecatedString& dep) {
  return std::wstring(reinterpret_cast<const wchar_t*>(dep.unicode()),
                      dep.length());
}

// URL conversions -------------------------------------------------------------

GURL KURLToGURL(const WebCore::KURL& url) {
#ifdef USE_GOOGLE_URL_LIBRARY
  const WebCore::CString& spec = url.utf8String();
  if (spec.isNull())
    return GURL();
  return GURL(spec.data(), spec.length(), url.parsed(), url.isValid());
#else
  return GURL(WideToUTF8(DeprecatedStringToStdWString(spec))); 
#endif
}

WebCore::KURL GURLToKURL(const GURL& url) {
  const std::string& spec = url.possibly_invalid_spec();
#ifdef USE_GOOGLE_URL_LIBRARY
  // Convert using the internal structures to avoid re-parsing.
  return WebCore::KURL(spec.c_str(), static_cast<int>(spec.length()),
                       url.parsed_for_possibly_invalid_spec(), url.is_valid());
#else
  return WebCore::KURL(StdWStringToDeprecatedString(UTF8ToWide(spec)));
#endif
}

}  // namespace webkit_glue
