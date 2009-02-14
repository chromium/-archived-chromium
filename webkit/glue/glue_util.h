// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_GLUE_UTIL_H_
#define WEBKIT_GLUE_GLUE_UTIL_H_

#include <string>

#include "base/file_path.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

namespace WebCore {
class CString;
class IntRect;
class KURL;
class String;
}

namespace gfx {
class Rect;
}

namespace webkit_glue {

// WebCore::CString <-> std::string. All characters are 8-bit and are preserved
// unchanged.
std::string CStringToStdString(const WebCore::CString& str);
WebCore::CString StdStringToCString(const std::string& str);

// WebCore::String <-> std::wstring. We assume that the WebCore::String is in
// UTF-16, and will either copy to a UTF-16 std::wstring (on Windows) or convert
// to a UTF-32 one on Linux and Mac.
std::wstring StringToStdWString(const WebCore::String& str);
WebCore::String StdWStringToString(const std::wstring& str);

// WebCore::String -> string16. This is a direct copy of UTF-16 characters.
string16 StringToString16(const WebCore::String& str);
WebCore::String String16ToString(const string16& str);

// WebCore::String <-> std::string. We assume the WebCore::String is UTF-16 and
// the std::string is UTF-8, and convert as necessary.
std::string StringToStdString(const WebCore::String& str);
WebCore::String StdStringToString(const std::string& str);

FilePath::StringType StringToFilePathString(const WebCore::String& str);
WebCore::String FilePathStringToString(const FilePath::StringType& str);

GURL KURLToGURL(const WebCore::KURL& url);
WebCore::KURL GURLToKURL(const GURL& url);
GURL StringToGURL(const WebCore::String& spec);

gfx::Rect FromIntRect(const WebCore::IntRect& r);
WebCore::IntRect ToIntRect(const gfx::Rect& r);

}  // namespace webkit_glue

#endif  // #ifndef WEBKIT_GLUE_GLUE_UTIL_H_

