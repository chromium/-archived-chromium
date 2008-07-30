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

#include <string>

class GURL;

namespace net {

// See RFC 2397 for a complete description of the 'data' URL scheme.
//
// Briefly, a 'data' URL has the form:
//
//   data:[<mediatype>][;base64],<data>
//
// The <mediatype> is an Internet media type specification (with optional
// parameters.)  The appearance of ";base64" means that the data is encoded as
// base64.  Without ";base64", the data (as a sequence of octets) is represented
// using ASCII encoding for octets inside the range of safe URL characters and
// using the standard %xx hex encoding of URLs for octets outside that range.
// If <mediatype> is omitted, it defaults to text/plain;charset=US-ASCII.  As a
// shorthand, "text/plain" can be omitted but the charset parameter supplied.
//
class DataURL {
 public:
  // This method can be used to parse a 'data' URL into its component pieces.
  //
  // The resulting mime_type is normalized to lowercase.  The data is the
  // decoded data (e.g.., if the data URL specifies base64 encoding, then the
  // returned data is base64 decoded, and any %-escaped bytes are unescaped).
  //
  // If the URL is malformed, then this method will return false, and its
  // output variables will remain unchanged.  On success, true is returned.
  //
  static bool Parse(const GURL& url,
                    std::string* mime_type,
                    std::string* charset,
                    std::string* data);
};

}  // namespace net
