// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_GLUE_UTIL_H_
#define WEBKIT_GLUE_GLUE_UTIL_H_

#include <string>

#include "base/string16.h"
#include "googleurl/src/gurl.h"

namespace WebCore {
  class CString;
  class DeprecatedString;
  class KURL;
  class String;
}

namespace webkit_glue {
  std::string CStringToStdString(const WebCore::CString& str);
  WebCore::CString StdStringToCString(const std::string& str);
  std::wstring StringToStdWString(const WebCore::String& str);
  std::string16 StringToStdString16(const WebCore::String& str);

  WebCore::String StdWStringToString(const std::wstring& str);
  WebCore::String StdStringToString(const std::string& str);
  
  WebCore::DeprecatedString StdWStringToDeprecatedString(const std::wstring& str);
  std::wstring DeprecatedStringToStdWString(const WebCore::DeprecatedString& dep);

  GURL KURLToGURL(const WebCore::KURL& url);
  WebCore::KURL GURLToKURL(const GURL& url);
}

#endif  // #ifndef WEBKIT_GLUE_GLUE_UTIL_H_

