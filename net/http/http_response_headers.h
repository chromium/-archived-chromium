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

#ifndef NET_HTTP_RESPONSE_HEADERS_H_
#define NET_HTTP_RESPONSE_HEADERS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"

class Pickle;
class Time;
class TimeDelta;

namespace net {

// HttpResponseHeaders: parses and holds HTTP response headers.
class HttpResponseHeaders :
    public base::RefCountedThreadSafe<HttpResponseHeaders> {
 public:
  // Parses the given raw_headers.  raw_headers should be formatted thus:
  // includes the http status response line, each line is \0-terminated, and
  // it's terminated by an empty line (ie, 2 \0s in a row).
  //
  // NOTE: For now, raw_headers is not really 'raw' in that this constructor is
  // called with a 'NativeMB' string on Windows because WinHTTP does not allow
  // us to access the raw byte sequence as sent by a web server.  In any case,
  // HttpResponseHeaders does not perform any encoding changes on the input.
  //
  explicit HttpResponseHeaders(const std::string& raw_headers);

  // Initializes from the representation stored in the given pickle.  The data
  // for this object is found relative to the given pickle_iter, which should
  // be passed to the pickle's various Read* methods.
  HttpResponseHeaders(const Pickle& pickle, void** pickle_iter);

  // Appends a representation of this object to the given pickle.  If the
  // for_cache argument is true, then non-cacheable headers will be pruned from
  // the persisted version of the response headers.
  void Persist(Pickle* pickle, bool for_cache);

  // Performs header merging as described in 13.5.3 of RFC 2616.
  void Update(const HttpResponseHeaders& new_headers);

  // Creates a normalized header string.  The output will be formatted exactly
  // like so:
  //     HTTP/<version> <status_code> <status_text>\n
  //     [<header-name>: <header-values>\n]*
  // meaning, each line is \n-terminated, and there is no extra whitespace
  // beyond the single space separators shown (of course, values can contain
  // whitespace within them).  If a given header-name appears more than once
  // in the set of headers, they are combined into a single line like so:
  //     <header-name>: <header-value1>, <header-value2>, ...<header-valueN>\n
  //
  // DANGER: For some headers (e.g., "Set-Cookie"), the normalized form can be
  // a lossy format.  This is due to the fact that some servers generate
  // Set-Cookie headers that contain unquoted commas (usually as part of the
  // value of an "expires" attribute).  So, use this function with caution.  Do
  // not expect to be able to re-parse Set-Cookie headers from this output.
  //
  // NOTE: Do not make any assumptions about the encoding of this output
  // string.  It may be non-ASCII, and the encoding used by the server is not
  // necessarily known to us.  Do not assume that this output is UTF-8!
  //
  // TODO(darin): remove this method
  //
  void GetNormalizedHeaders(std::string* output) const;

  // Fetch the "normalized" value of a single header, where all values for the
  // header name are separated by commas.  See the GetNormalizedHeaders for
  // format details.  Returns false if this header wasn't found.
  //
  // NOTE: Do not make any assumptions about the encoding of this output
  // string.  It may be non-ASCII, and the encoding used by the server is not
  // necessarily known to us.  Do not assume that this output is UTF-8!
  //
  // TODO(darin): remove this method
  //
  bool GetNormalizedHeader(const std::string& name, std::string* value) const;

  // Returns the normalized status line.  For HTTP/0.9 responses (i.e.,
  // responses that lack a status line), this is the manufactured string
  // "HTTP/0.9 200 OK".
  std::string GetStatusLine() const;

  // Enumerate the "lines" of the response headers.  This skips over the status
  // line.  Use GetStatusLine if you are interested in that.  Note that this
  // method returns the un-coalesced response header lines, so if a response
  // header appears on multiple lines, then it will appear multiple times in
  // this enumeration (in the order the header lines were received from the
  // server).  Initialize a 'void*' variable to NULL and pass it by address to
  // EnumerateHeaderLines.  Call EnumerateHeaderLines repeatedly until it
  // returns false.  The out-params 'name' and 'value' are set upon success.
  bool EnumerateHeaderLines(void** iter,
                            std::string* name,
                            std::string* value) const;

  // Enumerate the values of the specified header.   If you are only interested
  // in the first header, then you can pass NULL for the 'iter' parameter.
  // Otherwise, to iterate across all values for the specified header,
  // initialize a 'void*' variable to NULL and pass it by address to
  // EnumerateHeader.  Call EnumerateHeader repeatedly until it returns false.
  bool EnumerateHeader(void** iter,
                       const std::string& name,
                       std::string* value) const;

  // Returns true if the response contains the specified header-value pair.
  // Both name and value are compared case insensitively.
  bool HasHeaderValue(const std::string& name, const std::string& value) const;

  // Get the mime type and charset values in lower case form from the headers.
  // Empty strings are returned if the values are not present.
  void GetMimeTypeAndCharset(std::string* mime_type,
                             std::string* charset) const;

  // Get the mime type in lower case from the headers.  If there's no mime
  // type, returns false.
  bool GetMimeType(std::string* mime_type) const;

  // Get the charset in lower case from the headers.  If there's no charset,
  // returns false.
  bool GetCharset(std::string* charset) const;

  // Returns true if this response corresponds to a redirect.  The target
  // location of the redirect is optionally returned if location is non-null.
  bool IsRedirect(std::string* location) const;

  // Returns true if the response cannot be reused without validation.  The
  // result is relative to the current_time parameter, which is a parameter to
  // support unit testing.  The request_time parameter indicates the time at
  // which the request was made that resulted in this response, which was
  // received at response_time.
  bool RequiresValidation(const Time& request_time,
                          const Time& response_time,
                          const Time& current_time) const;

  // Returns the amount of time the server claims the response is fresh from
  // the time the response was generated.  See section 13.2.4 of RFC 2616.  See
  // RequiresValidation for a description of the response_time parameter.
  TimeDelta GetFreshnessLifetime(const Time& response_time) const;

  // Returns the age of the response.  See section 13.2.3 of RFC 2616.
  // See RequiresValidation for a description of this method's parameters.
  TimeDelta GetCurrentAge(const Time& request_time,
                          const Time& response_time,
                          const Time& current_time) const;

  // The following methods extract values from the response headers.  If a
  // value is not present, then false is returned.  Otherwise, true is returned
  // and the out param is assigned to the corresponding value.
  bool GetMaxAgeValue(TimeDelta* value) const;
  bool GetAgeValue(TimeDelta* value) const;
  bool GetDateValue(Time* value) const;
  bool GetLastModifiedValue(Time* value) const;
  bool GetExpiresValue(Time* value) const;

  // Extracts the time value of a particular header.  This method looks for the
  // first matching header value and parses its value as a HTTP-date.
  bool GetTimeValuedHeader(const std::string& name, Time* result) const;

  // Determines if this response indicates a keep-alive connection.
  bool IsKeepAlive() const;

  // Extracts the value of the Content-Length header or returns -1 if there is
  // no such header in the response.
  int64 GetContentLength() const;

  // Returns the HTTP response code.  This is 0 if the response code text seems
  // to exist but could not be parsed.  Otherwise, it defaults to 200 if the
  // response code is not found in the raw headers.
  int response_code() const { return response_code_; }

  // Returns the raw header string.
  const std::string& raw_headers() const { return raw_headers_; }

 private:
  friend class base::RefCountedThreadSafe<HttpResponseHeaders>;

  HttpResponseHeaders() {}
  ~HttpResponseHeaders() {}

  // Initializes from the given raw headers.
  void Parse(const std::string& raw_input);

  // Helper function for ParseStatusLine.
  // Tries to extract the "HTTP/X.Y" from a status line formatted like:
  //    HTTP/1.1 200 OK
  // with line_begin and end pointing at the begin and end of this line.  If the
  // status line is malformed, we'll guess a version number.
  // Output will be a normalized version of this, with a trailing \n.
  void ParseVersion(std::string::const_iterator line_begin,
                    std::string::const_iterator line_end);

  // Tries to extract the status line from a header block, given the first
  // line of said header block.  If the status line is malformed, we'll construct
  // a valid one.  Example input:
  //    HTTP/1.1 200 OK
  // with line_begin and end pointing at the begin and end of this line.
  // Output will be a normalized version of this, with a trailing \n.
  void ParseStatusLine(std::string::const_iterator line_begin,
                       std::string::const_iterator line_end);

  // Tries to extract the header line from a header block, given a single
  // line of said header block.  If the header is malformed, we skip it.
  // Example input:
  //    Content-Length : text/html; charset=utf-8
  void ParseHeaderLine(std::string::const_iterator line_begin,
                       std::string::const_iterator line_end);

  // Find the header in our list (case-insensitive) starting with parsed_ at
  // index |from|.  Returns string::npos if not found.
  size_t FindHeader(size_t from, const std::string& name) const;

  // Add a header->value pair to our list.  If we already have header in our list,
  // append the value to it.
  void AddHeader(std::string::const_iterator name_begin,
                 std::string::const_iterator name_end,
                 std::string::const_iterator value_begin,
                 std::string::const_iterator value_end);

  // Add to parsed_ given the fields of a ParsedHeader object.
  void AddToParsed(std::string::const_iterator name_begin,
                   std::string::const_iterator name_end,
                   std::string::const_iterator value_begin,
                   std::string::const_iterator value_end);

  typedef base::hash_set<std::string> HeaderSet;

  // Returns the values from any 'cache-control: no-cache="foo,bar"' headers as
  // well as other known-to-be-transient header names.  The header names are
  // all lowercase to support fast lookup.
  void GetTransientHeaders(HeaderSet* header_names) const;

  // The members of this structure point into raw_headers_.
  struct ParsedHeader {
    std::string::const_iterator name_begin;
    std::string::const_iterator name_end;
    std::string::const_iterator value_begin;
    std::string::const_iterator value_end;

    // A header "continuation" contains only a subsequent value for the
    // preceding header.  (Header values are comma separated.)
    bool is_continuation() const { return name_begin == name_end; }
  };
  typedef std::vector<ParsedHeader> HeaderList;

  // We keep a list of ParsedHeader objects.  These tell us where to locate the
  // header-value pairs within raw_headers_.
  HeaderList parsed_;

  // The raw_headers_ consists of the normalized status line (terminated with a
  // null byte) and then followed by the raw null-terminated headers from the
  // input that was passed to our constructor.  We preserve the input to
  // maintain as much ancillary fidelity as possible (since it is sometimes
  // hard to tell what may matter down-stream to a consumer of XMLHttpRequest).
  std::string raw_headers_;

  // This is the parsed HTTP response code.
  int response_code_;

  DISALLOW_COPY_AND_ASSIGN(HttpResponseHeaders);
};

}  // namespace net

#endif  // NET_HTTP_RESPONSE_HEADERS_H_
