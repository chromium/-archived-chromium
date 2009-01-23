// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_UTIL_H_
#define NET_HTTP_HTTP_UTIL_H_

#include "base/string_tokenizer.h"
#include "googleurl/src/gurl.h"

// This is a macro to support extending this string literal at compile time.
// Please excuse me polluting your global namespace!
#define HTTP_LWS " \t"

namespace net {

class HttpUtil {
 public:
   // Returns the absolute path of the URL, to be used for the http request.
   // The absolute path starts with a '/' and may contain a query.
   static std::string PathForRequest(const GURL& url);

   // Returns the absolute URL, to be used for the http request. This url is
   // made up of the protocol, host, [port], path, [query]. Everything else
   // is stripped (username, password, reference).
   static std::string SpecForRequest(const GURL& url);

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

  // Return true if the character is HTTP "linear white space" (SP | HT).
  // This definition corresponds with the HTTP_LWS macro, and does not match
  // newlines.
  static bool IsLWS(char c);

  // Trim HTTP_LWS chars from the beginning and end of the string.
  static void TrimLWS(std::string::const_iterator* begin,
                      std::string::const_iterator* end);

  // Whether the character is the start of a quotation mark.
  static bool IsQuote(char c);

  // RFC 2616 Sec 2.2:
  // quoted-string = ( <"> *(qdtext | quoted-pair ) <"> )
  // Unquote() strips the surrounding quotemarks off a string, and unescapes
  // any quoted-pair to obtain the value contained by the quoted-string.
  // If the input is not quoted, then it works like the identity function.
  static std::string Unquote(std::string::const_iterator begin,
                             std::string::const_iterator end);

  // Same as above.
  static std::string Unquote(const std::string& str);

  // The reverse of Unquote() -- escapes and surrounds with "
  static std::string Quote(const std::string& str);

  // Returns the start of the status line, or -1 if no status line was found.
  // This allows for 4 bytes of junk to precede the status line (which is what
  // mozilla does too).
  static int LocateStartOfStatusLine(const char* buf, int buf_len);

  // Returns index beyond the end-of-headers marker or -1 if not found.  RFC
  // 2616 defines the end-of-headers marker as a double CRLF; however, some
  // servers only send back LFs (e.g., Unix-based CGI scripts written using the
  // ASIS Apache module).  This function therefore accepts the pattern LF[CR]LF
  // as end-of-headers (just like Mozilla).
  // The parameter |i| is the offset within |buf| to begin searching from.
  static int LocateEndOfHeaders(const char* buf, int buf_len, int i = 0);

  // Assemble "raw headers" in the format required by HttpResponseHeaders.
  // This involves normalizing line terminators, converting [CR]LF to \0 and
  // handling HTTP line continuations (i.e., lines starting with LWS are
  // continuations of the previous line).  |buf_len| indicates the position of
  // the end-of-headers marker as defined by LocateEndOfHeaders.
  static std::string AssembleRawHeaders(const char* buf, int buf_len);

  // Given a comma separated ordered list of language codes, return
  // the list with a qvalue appended to each language.
  // The way qvalues are assigned is rather simple. The qvalue
  // starts with 1.0 and is decremented by 0.2 for each successive entry
  // in the list until it reaches 0.2. All the entries after that are
  // assigned the same qvalue of 0.2. Also, note that the 1st language
  // will not have a qvalue added because the absence of a qvalue implicitly 
  // means q=1.0.
  // 
  // When making a http request, this should be used to determine what
  // to put in Accept-Language header. If a comma separated list of language
  // codes *without* qvalue is sent, web servers regard all
  // of them as having q=1.0 and pick one of them even though it may not
  // be at the beginning of the list (see http://crbug.com/5899).
  static std::string GenerateAcceptLanguageHeader(
      const std::string& raw_language_list);

  // Given a charset, return the list with a qvalue. If charset is utf-8,
  // it will return 'utf-8,*;q=0.5'. Otherwise (e.g. 'euc-jp'), it'll return
  // 'euc-jp,utf-8;q=0.7,*;q=0.3'.
  static std::string GenerateAcceptCharsetHeader(const std::string& charset);

  // Used to iterate over the name/value pairs of HTTP headers.  To iterate
  // over the values in a multi-value header, use ValuesIterator.
  // See AssembleRawHeaders for joining line continuations (this iterator
  // does not expect any).
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

