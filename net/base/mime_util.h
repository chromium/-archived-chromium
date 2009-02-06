// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MIME_UTIL_H__
#define NET_BASE_MIME_UTIL_H__

#include <string>

namespace net {

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists.
bool GetMimeTypeFromExtension(const std::wstring& ext, std::string* mime_type);

// Get the mime type (if any) that is associated with the given file.  Returns
// true if a corresponding mime type exists.
bool GetMimeTypeFromFile(const std::wstring& file_path, std::string* mime_type);

// Get the preferred extension (if any) associated with the given mime type.
// Returns true if a corresponding file extension exists.  The extension is
// returned without a prefixed dot, ex "html".
bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::wstring* extension);

// Check to see if a particular MIME type is in our list.
bool IsSupportedImageMimeType(const char* mime_type);
bool IsSupportedNonImageMimeType(const char* mime_type);
bool IsSupportedJavascriptMimeType(const char* mime_type);

// Get whether this mime type should be displayed in view-source mode.
// (For example, XML.)
bool IsViewSourceMimeType(const char* mime_type);

// Convenience function.
bool IsSupportedMimeType(const std::string& mime_type);

// Returns true if this the mime_type_pattern matches a given mime-type.
// Checks for absolute matching and wildcards.  mime-types should be in
// lower case.
bool MatchesMimeType(const std::string &mime_type_pattern,
                     const std::string &mime_type);

}  // namespace net

#endif  // NET_BASE_MIME_UTIL_H__

