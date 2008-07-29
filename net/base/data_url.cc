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

// NOTE: based loosely on mozilla's nsDataChannel.cpp

#include <algorithm>

#include "net/base/data_url.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/base64.h"
#include "net/base/escape.h"

/*static*/
bool DataURL::Parse(const GURL& url, std::string* mime_type,
                    std::string* charset, std::string* data) {
  std::string::const_iterator begin = url.spec().begin();
  std::string::const_iterator end = url.spec().end();

  std::string::const_iterator after_colon = std::find(begin, end, ':');
  if (after_colon == end)
    return false;
  ++after_colon;

  // first, find the start of the data
  std::string::const_iterator comma = std::find(after_colon, end, ',');
  if (comma == end)
    return false;

  const char kBase64Tag[] = ";base64";
  std::string::const_iterator it =
      std::search(after_colon, comma, kBase64Tag,
                  kBase64Tag + sizeof(kBase64Tag)-1);

  bool base64_encoded = (it != comma);

  if (comma != after_colon) {
    // everything else is content type
    std::string::const_iterator semi_colon = std::find(after_colon, comma, ';');
    if (semi_colon != after_colon) {
      mime_type->assign(after_colon, semi_colon);
      StringToLowerASCII(mime_type);
    }
    if (semi_colon != comma) {
      const char kCharsetTag[] = "charset=";
      it = std::search(semi_colon + 1, comma, kCharsetTag,
                       kCharsetTag + sizeof(kCharsetTag)-1);
      if (it != comma)
        charset->assign(it + sizeof(kCharsetTag)-1, comma);
    }
  }

  // fallback to defaults if nothing specified in the URL:
  if (mime_type->empty())
    mime_type->assign("text/plain");
  if (charset->empty())
    charset->assign("US-ASCII");

  // Preserve spaces if dealing with text or xml input, same as mozilla:
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=138052
  // but strip them otherwise:
  //   https://bugzilla.mozilla.org/show_bug.cgi?id=37200
  // (Spaces in a data URL should be escaped, which is handled below, so any
  // spaces now are wrong. People expect to be able to enter them in the URL
  // bar for text, and it can't hurt, so we allow it.)
  std::string temp_data = std::string(comma + 1, end);

  // For base64, we may have url-escaped whitespace which is not part
  // of the data, and should be stripped. Otherwise, the escaped whitespace
  // could be part of the payload, so don't strip it.
  if (base64_encoded) {
    temp_data = UnescapeURLComponent(temp_data,
        UnescapeRule::SPACES | UnescapeRule::PERCENTS);
  }

  // Strip whitespace.
  if (base64_encoded || !(mime_type->compare(0, 5, "text/") == 0 ||
                          mime_type->find("xml") != std::string::npos)) {
    temp_data.erase(std::remove_if(temp_data.begin(), temp_data.end(),
                                   IsAsciiWhitespace<wchar_t>),
                    temp_data.end());
  }

  if (!base64_encoded) {
    temp_data = UnescapeURLComponent(temp_data,
        UnescapeRule::SPACES | UnescapeRule::PERCENTS);
  }

  if (base64_encoded)
    return Base64Decode(temp_data, data);

  temp_data.swap(*data);
  return true;
}
