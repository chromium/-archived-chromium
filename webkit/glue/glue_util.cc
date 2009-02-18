// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "base/compiler_specific.h"

#include "webkit/glue/glue_util.h"

#include <string>

MSVC_PUSH_WARNING_LEVEL(0);
#include "CString.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "KURL.h"
MSVC_POP_WARNING();

#undef LOG

#include "base/compiler_specific.h"
#include "base/gfx/rect.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"


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
#if defined(WCHAR_T_IS_UTF16)
  return std::wstring(chars ? chars : L"", str.length());
#elif defined(WCHAR_T_IS_UTF32)
  string16 str16(chars ? chars : reinterpret_cast<const char16 *>(L""),
                                 str.length());
  return UTF16ToWide(str16);
#endif
}

string16 StringToString16(const WebCore::String& str) {
  const UChar* chars = str.characters();
  return string16(chars ? chars : (UChar *)L"", str.length());
}

WebCore::String String16ToString(const string16& str) {
  return WebCore::String(reinterpret_cast<const UChar*>(str.data()),
                         str.length());
}

std::string StringToStdString(const WebCore::String& str) {
  if (str.length() == 0)
    return std::string();
  std::string ret;
  UTF16ToUTF8(str.characters(), str.length(), &ret);
  return ret;
}

WebCore::String StdWStringToString(const std::wstring& str) {
#if defined(WCHAR_T_IS_UTF16)
  return WebCore::String(str.data(), static_cast<unsigned>(str.length()));
#elif defined(WCHAR_T_IS_UTF32)
  string16 str16 = WideToUTF16(str);
  return WebCore::String(str16.data(), static_cast<unsigned>(str16.length()));
#endif
}

WebCore::String StdStringToString(const std::string& str) {
  return WebCore::String::fromUTF8(str.data(),
                                   static_cast<unsigned>(str.length()));
}

FilePath::StringType StringToFilePathString(const WebCore::String& str) {
#if defined(OS_WIN)
  return StringToStdWString(str);
#elif defined(OS_POSIX)
  return base::SysWideToNativeMB(StringToStdWString(str));
#endif
}

WebCore::String FilePathStringToString(const FilePath::StringType& str) {
#if defined(OS_WIN)
  return StdWStringToString(str);
#elif defined(OS_POSIX)
  return StdWStringToString(base::SysNativeMBToWide(str));
#endif
}

// URL conversions -------------------------------------------------------------

GURL KURLToGURL(const WebCore::KURL& url) {
#if USE(GOOGLEURL)
  const WebCore::CString& spec = url.utf8String();
  if (spec.isNull() || 0 == spec.length())
    return GURL();
  return GURL(spec.data(), spec.length(), url.parsed(), url.isValid());
#else
  return StringToGURL(url.string());
#endif
}

WebCore::KURL GURLToKURL(const GURL& url) {
  const std::string& spec = url.possibly_invalid_spec();
#if USE(GOOGLEURL)
  // Convert using the internal structures to avoid re-parsing.
  return WebCore::KURL(WebCore::CString(spec.c_str(),
                                        static_cast<unsigned>(spec.length())),
                       url.parsed_for_possibly_invalid_spec(), url.is_valid());
#else
  return WebCore::KURL(StdWStringToString(UTF8ToWide(spec)));
#endif
}

GURL StringToGURL(const WebCore::String& spec) {
  return GURL(WideToUTF8(StringToStdWString(spec)));
}

// Rect conversions ------------------------------------------------------------

gfx::Rect FromIntRect(const WebCore::IntRect& r) {
    return gfx::Rect(r.x(), r.y(), r.width() < 0 ? 0 : r.width(),
        r.height() < 0 ? 0 : r.height());
}

WebCore::IntRect ToIntRect(const gfx::Rect& r) {
  return WebCore::IntRect(r.x(), r.y(), r.width(), r.height());
}

}  // namespace webkit_glue

