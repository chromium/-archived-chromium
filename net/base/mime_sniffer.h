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

#ifndef NET_BASE_MIME_SNIFFER_H__
#define NET_BASE_MIME_SNIFFER_H__

#include <string>

class GURL;

namespace mime_util {

// Examine the URL and the mime_type and decide whether we should sniff a
// replacement mime type from the content.
//
// @param url The URL from which we obtained the content.
// @param mime_type The current mime type, e.g. from the Content-Type header.
// @return Returns true if we should sniff the mime type.
bool ShouldSniffMimeType(const GURL& url, const std::string& mime_type);

// Guess a mime type from the first few bytes of content an its URL.  Always
// assigns |result| with its best guess of a mime type.
//
// @param content A buffer containing the bytes to sniff.
// @param content_size The number of bytes in the |content| buffer.
// @param url The URL from which we obtained this content.
// @param type_hint The current mime type, e.g. from the Content-Type header.
// @param result Address at which to place the sniffed mime type.
// @return Returns true if we have enough content to guess the mime type.
bool SniffMimeType(const char* content, size_t content_size,
                   const GURL& url, const std::string& type_hint,
                   std::string* result);

}  // namespace mime_util

#endif  // NET_BASE_MIME_SNIFFER_H__
