// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MIME_SNIFFER_H__
#define NET_BASE_MIME_SNIFFER_H__

#include <string>

class GURL;

namespace net {

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

}  // namespace net

#endif  // NET_BASE_MIME_SNIFFER_H__
