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

#ifndef NET_HTTP_HTTP_UTIL_H_
#define NET_HTTP_HTTP_UTIL_H_

#include "base/string_tokenizer.h"

// This is a macro to support extending this string literal at compile time.
// Please excuse me polluting your global namespace!
#define HTTP_LWS " \t"

namespace net {

class HttpUtil {
 public:
  // Locates the next occurance of delimiter in line, skipping over quoted
  // strings (e.g., commas will not be treated as delimiters if they appear
  // within a quoted string).  Returns the offset of the found delimiter or
  // line.size() if no delimiter was found.
  static size_t FindDelimiter(const std::string& line,
                              size_t search_start,
                              char delimiter);

  // Parses the value of a Content-Type header.  The resulting mime_type and
  // charset values are normalized to lowercase.  The mime_type and charset
  // output values are only modified if the content_type_str contains a mime
  // type and charset value, respectively.
  static void ParseContentType(const std::string& content_type_str,
                               std::string* mime_type,
                               std::string* charset,
                               bool *had_charset);

  // Scans the '\r\n'-delimited headers for the given header name.  Returns
  // true if a match is found.  Input is assumed to be well-formed.
  // TODO(darin): kill this
  static bool HasHeader(const std::string& headers, const char* name);

  // Multiple occurances of some headers cannot be coalesced into a comma-
  // separated list since their values are (or contain) unquoted HTTP-date
  // values, which may contain a comma (see RFC 2616 section 3.3.1).
  static bool IsNonCoalescingHeader(std::string::const_iterator name_begin,
                                    std::string::const_iterator name_end);
  static bool IsNonCoalescingHeader(const std::string& name) {
    return IsNonCoalescingHeader(name.begin(), name.end());
  }

  // Trim HTTP_LWS chars from the beginning and end of the string.
  static void TrimLWS(std::string::const_iterator* begin,
                      std::string::const_iterator* end);

  // Returns index beyond the end-of-headers marker or -1 if not found.  RFC
  // 2616 defines the end-of-headers marker as a double CRLF; however, some
  // servers only send back LFs (e.g., Unix-based CGI scripts written using the
  // ASIS Apache module).  This function therefore accepts the pattern LF[CR]LF
  // as end-of-headers (just like Mozilla).
  static int LocateEndOfHeaders(const char* buf, int buf_len);

  // Assemble "raw headers" in the format required by HttpResponseHeaders.
  // This involves normalizing line terminators, converting [CR]LF to \0 and
  // handling HTTP line continuations (i.e., lines starting with LWS are
  // continuations of the previous line).  |buf_len| indicates the position of
  // the end-of-headers marker as defined by LocateEndOfHeaders.
  static std::string AssembleRawHeaders(const char* buf, int buf_len);

  // Used to iterate over the name/value pairs of HTTP headers.  To iterate
  // over the values in a multi-value header, use ValuesIterator.
  class HeadersIterator {
   public:
    HeadersIterator(std::string::const_iterator headers_begin,
                    std::string::const_iterator headers_end,
                    const std::string& line_delimiter);

    // Advances the iterator to the next header, if any.  Returns true if there
    // is a next header.  Use name* and values* methods to access the resultant
    // header name and values.
    bool GetNext();

    std::string::const_iterator name_begin() const {
      return name_begin_;
    }
    std::string::const_iterator name_end() const {
      return name_end_;
    }
    std::string name() const {
      return std::string(name_begin_, name_end_);
    }

    std::string::const_iterator values_begin() const {
      return values_begin_;
    }
    std::string::const_iterator values_end() const {
      return values_end_;
    }
    std::string values() const {
      return std::string(values_begin_, values_end_);
    }

   private:
    StringTokenizer lines_;
    std::string::const_iterator name_begin_;
    std::string::const_iterator name_end_;
    std::string::const_iterator values_begin_;
    std::string::const_iterator values_end_;
  };

  // Used to iterate over deliminated values in a HTTP header.  HTTP LWS is
  // automatically trimmed from the resulting values.
  //
  // When using this class to iterate over response header values, beware that
  // for some headers (e.g., Last-Modified), commas are not used as delimiters.
  // This iterator should be avoided for headers like that which are considered
  // non-coalescing (see IsNonCoalescingHeader).
  //
  // This iterator is careful to skip over delimiters found inside an HTTP
  // quoted string.
  //
  class ValuesIterator {
   public:
    ValuesIterator(std::string::const_iterator values_begin,
                   std::string::const_iterator values_end,
                   char delimiter);

    // Advances the iterator to the next value, if any.  Returns true if there
    // is a next value.  Use value* methods to access the resultant value.
    bool GetNext();

    std::string::const_iterator value_begin() const {
      return value_begin_;
    }
    std::string::const_iterator value_end() const {
      return value_end_;
    }
    std::string value() const {
      return std::string(value_begin_, value_end_);
    }

   private:
    StringTokenizer values_;
    std::string::const_iterator value_begin_;
    std::string::const_iterator value_end_;
  };
};

}  // namespace net

#endif  // NET_HTTP_HTTP_UTIL_H_
