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

#ifndef NET_BASE_MIME_UTIL_H__
#define NET_BASE_MIME_UTIL_H__

#include <string>

namespace mime_util {

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists.
bool GetMimeTypeFromExtension(const std::wstring& ext, std::string* mime_type);

// Get the mime type (if any) that is associated with the given file.  Returns
// true if a corresponding mime type exists.
bool GetMimeTypeFromFile(const std::wstring& file_path, std::string* mime_type);

// Get the preferred extension (if any) associated with the given mime type.
// Returns true if a corresponding file extension exists.  The extension is
// returned with a prefixed dot (as stored in the registry), ex ".avi".
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

}  // namespace mime_util

#endif  // NET_BASE_MIME_UTIL_H__
