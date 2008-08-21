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

// The rules for header parsing were borrowed from Firefox:
// http://lxr.mozilla.org/seamonkey/source/netwerk/protocol/http/src/nsHttpResponseHead.cpp
// The rules for parsing content-types were also borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_response_headers.h"

#include <algorithm>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/escape.h"
#include "net/http/http_util.h"

using std::string;

namespace net {

//-----------------------------------------------------------------------------

namespace {

// These response headers are not persisted in a cached representation of the
// response headers.  (This list comes from Mozilla's nsHttpResponseHead.cpp)
const char* kTransientHeaders[] = {
  "connection",
  "proxy-connection",
  "keep-alive",
  "www-authenticate",
  "proxy-authenticate",
  "trailer",
  "transfer-encoding",
  "upgrade",
  "set-cookie",
  "set-cookie2"
};

// These respones headers are not copied from a 304/206 response to the cached
// response headers.  This list is based on Mozilla's nsHttpResponseHead.cpp.
const char* kNonUpdatedHeaders[] = {
  "connection",
  "proxy-connection",
  "keep-alive",
  "www-authenticate",
  "proxy-authenticate",
  "trailer",
  "transfer-encoding",
  "upgrade",
  // these should never change:
  "content-location",
  "content-md5",
  "etag",
  // assume cache-control: no-transform
  "content-encoding",
  "content-range",
  "content-type",
  // some broken microsoft servers send 'content-length: 0' with 304s
  "content-length"
};

bool ShouldUpdateHeader(const string::const_iterator& name_begin,
                        const string::const_iterator& name_end) {
  for (size_t i = 0; i < arraysize(kNonUpdatedHeaders); ++i) {
    if (LowerCaseEqualsASCII(name_begin, name_end, kNonUpdatedHeaders[i]))
      return false;
  }
  return true;
}

}  // namespace

//-----------------------------------------------------------------------------

HttpResponseHeaders::HttpResponseHeaders(const string& raw_input)
    : response_code_(-1) {
  Parse(raw_input);
}

HttpResponseHeaders::HttpResponseHeaders(const Pickle& pickle, void** iter)
    : response_code_(-1) {
  string raw_input;
  if (pickle.ReadString(iter, &raw_input))
    Parse(raw_input);
}

void HttpResponseHeaders::Persist(Pickle* pickle, bool for_cache) {
  if (for_cache) {
    HeaderSet transient_headers;
    GetTransientHeaders(&transient_headers);

    std::string blob;
    blob.reserve(raw_headers_.size());

    // this just copies the status line w/ terminator
    blob.assign(raw_headers_.c_str(), strlen(raw_headers_.c_str()) + 1);

    for (size_t i = 0; i < parsed_.size(); ++i) {
      DCHECK(!parsed_[i].is_continuation());

      // locate the start of the next header
      size_t k = i;
      while (++k < parsed_.size() && parsed_[k].is_continuation());
      --k;

      string header_name(parsed_[i].name_begin, parsed_[i].name_end);
      StringToLowerASCII(&header_name);

      if (transient_headers.find(header_name) == transient_headers.end()) {
        // includes terminator
        blob.append(parsed_[i].name_begin, parsed_[k].value_end + 1);
      }

      i = k;
    }
    blob.push_back('\0');

    pickle->WriteString(blob);
  } else {
    pickle->WriteString(raw_headers_);
  }
}

void HttpResponseHeaders::Update(const HttpResponseHeaders& new_headers) {
  DCHECK(new_headers.response_code() == 304 ||
         new_headers.response_code() == 206);

  // copy up to the null byte.  this just copies the status line.
  string new_raw_headers(raw_headers_.c_str());
  new_raw_headers.push_back('\0');

  HeaderSet updated_headers;

  // NOTE: we write the new headers then the old headers for convenience.  the
  // order should not matter.

  // figure out which headers we want to take from new_headers:
  for (size_t i = 0; i < new_headers.parsed_.size(); ++i) {
    const HeaderList& new_parsed = new_headers.parsed_;

    DCHECK(!new_parsed[i].is_continuation());

    // locate the start of the next header
    size_t k = i;
    while (++k < new_parsed.size() && new_parsed[k].is_continuation());
    --k;

    const string::const_iterator& name_begin = new_parsed[i].name_begin;
    const string::const_iterator& name_end = new_parsed[i].name_end;
    if (ShouldUpdateHeader(name_begin, name_end)) {
      string name(name_begin, name_end);
      StringToLowerASCII(&name);
      updated_headers.insert(name);

      // preserve this header line in the merged result (including trailing '\0')
      new_raw_headers.append(name_begin, new_parsed[k].value_end + 1);
    }

    i = k;
  }

  // now, build the new raw headers
  for (size_t i = 0; i < parsed_.size(); ++i) {
    DCHECK(!parsed_[i].is_continuation());

    // locate the start of the next header
    size_t k = i;
    while (++k < parsed_.size() && parsed_[k].is_continuation());
    --k;

    string name(parsed_[i].name_begin, parsed_[i].name_end);
    StringToLowerASCII(&name);
    if (updated_headers.find(name) == updated_headers.end()) {
      // ok to preserve this header in the final result
      new_raw_headers.append(parsed_[i].name_begin, parsed_[k].value_end + 1);
    }

    i = k;
  }
  new_raw_headers.push_back('\0');

  // ok, make this object hold the new data
  raw_headers_.clear();
  parsed_.clear();
  Parse(new_raw_headers);
}

void HttpResponseHeaders::Parse(const string& raw_input) {
  raw_headers_.reserve(raw_input.size());

  // ParseStatusLine adds a normalized status line to raw_headers_
  string::const_iterator line_begin = raw_input.begin();
  string::const_iterator line_end = find(line_begin, raw_input.end(), '\0');
  ParseStatusLine(line_begin, line_end);

  if (line_end == raw_input.end()) {
    raw_headers_.push_back('\0');
    return;
  }

  // Including a terminating null byte.
  size_t status_line_len = raw_headers_.size();

  // Now, we add the rest of the raw headers to raw_headers_, and begin parsing
  // it (to populate our parsed_ vector).
  raw_headers_.append(line_end + 1, raw_input.end());

  // Adjust to point at the null byte following the status line
  line_end = raw_headers_.begin() + status_line_len - 1;

  HttpUtil::HeadersIterator headers(line_end + 1, raw_headers_.end(),
                                    string(1, '\0'));
  while (headers.GetNext()) {
    AddHeader(headers.name_begin(),
              headers.name_end(),
              headers.values_begin(),
              headers.values_end());
  }
}

// Append all of our headers to the final output string.
void HttpResponseHeaders::GetNormalizedHeaders(string* output) const {
  // copy up to the null byte.  this just copies the status line.
  output->assign(raw_headers_.c_str());

  // headers may appear multiple times (not necessarily in succession) in the
  // header data, so we build a map from header name to generated header lines.
  // to preserve the order of the original headers, the actual values are kept
  // in a separate list.  finally, the list of headers is flattened to form
  // the normalized block of headers.
  //
  // NOTE: We take special care to preserve the whitespace around any commas
  // that may occur in the original response headers.  Because our consumer may
  // be a web app, we cannot be certain of the semantics of commas despite the
  // fact that RFC 2616 says that they should be regarded as value separators.
  //
  typedef base::hash_map<string, size_t> HeadersMap;
  HeadersMap headers_map;
  HeadersMap::iterator iter = headers_map.end();

  std::vector<string> headers;

  for (size_t i = 0; i < parsed_.size(); ++i) {
    DCHECK(!parsed_[i].is_continuation());

    string name(parsed_[i].name_begin, parsed_[i].name_end);
    string lower_name = StringToLowerASCII(name);

    iter = headers_map.find(lower_name);
    if (iter == headers_map.end()) {
      iter = headers_map.insert(
          HeadersMap::value_type(lower_name, headers.size())).first;
      headers.push_back(name + ": ");
    } else {
      headers[iter->second].append(", ");
    }

    string::const_iterator value_begin = parsed_[i].value_begin;
    string::const_iterator value_end = parsed_[i].value_end;
    while (++i < parsed_.size() && parsed_[i].is_continuation())
      value_end = parsed_[i].value_end;
    --i;

    headers[iter->second].append(value_begin, value_end);
  }

  for (size_t i = 0; i < headers.size(); ++i) {
    output->push_back('\n');
    output->append(headers[i]);
  }

  output->push_back('\n');
}

bool HttpResponseHeaders::GetNormalizedHeader(const string& name,
                                              string* value) const {
  // If you hit this assertion, please use EnumerateHeader instead!
  DCHECK(!HttpUtil::IsNonCoalescingHeader(name));

  value->clear();

  bool found = false;
  size_t i = 0;
  while (i < parsed_.size()) {
    i = FindHeader(i, name);
    if (i == string::npos)
      break;

    found = true;

    if (!value->empty())
      value->append(", ");

    string::const_iterator value_begin = parsed_[i].value_begin;
    string::const_iterator value_end = parsed_[i].value_end;
    while (++i < parsed_.size() && parsed_[i].is_continuation())
      value_end = parsed_[i].value_end;
    value->append(value_begin, value_end);
  }

  return found;
}

string HttpResponseHeaders::GetStatusLine() const {
  // copy up to the null byte.
  return string(raw_headers_.c_str());
}

bool HttpResponseHeaders::EnumerateHeaderLines(void** iter,
                                               string* name,
                                               string* value) const {
  size_t i = reinterpret_cast<size_t>(*iter);
  if (i == parsed_.size())
    return false;

  DCHECK(!parsed_[i].is_continuation());

  name->assign(parsed_[i].name_begin, parsed_[i].name_end);

  string::const_iterator value_begin = parsed_[i].value_begin;
  string::const_iterator value_end = parsed_[i].value_end;
  while (++i < parsed_.size() && parsed_[i].is_continuation())
    value_end = parsed_[i].value_end;

  value->assign(value_begin, value_end);

  *iter = reinterpret_cast<void*>(i);
  return true;
}

bool HttpResponseHeaders::EnumerateHeader(void** iter, const string& name,
                                          string* value) const {
  size_t i;
  if (!iter || !*iter) {
    i = FindHeader(0, name);
  } else {
    i = reinterpret_cast<size_t>(*iter);
    if (i >= parsed_.size()) {
      i = string::npos;
    } else if (!parsed_[i].is_continuation()) {
      i = FindHeader(i, name);
    }
  }

  if (i == string::npos) {
    value->clear();
    return false;
  }

  if (iter)
    *iter = reinterpret_cast<void*>(i + 1);
  value->assign(parsed_[i].value_begin, parsed_[i].value_end);
  return true;
}

bool HttpResponseHeaders::HasHeaderValue(const std::string& name,
                                         const std::string& value) const {
  // The value has to be an exact match.  This is important since
  // 'cache-control: no-cache' != 'cache-control: no-cache="foo"'
  void* iter = NULL;
  std::string temp;
  while (EnumerateHeader(&iter, name, &temp)) {
    if (value.size() == temp.size() &&
        std::equal(temp.begin(), temp.end(), value.begin(),
                   CaseInsensitiveCompare<char>()))
      return true;
  }
  return false;
}

// Note: this implementation implicitly assumes that line_end points at a valid
// sentinel character (such as '\0').
void HttpResponseHeaders::ParseVersion(string::const_iterator line_begin,
                                       string::const_iterator line_end) {
  string::const_iterator p = line_begin;

  // RFC2616 sec 3.1: HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
  // (1*DIGIT apparently means one or more digits, but we only handle 1).

  if ((line_end - p < 4) || !LowerCaseEqualsASCII(p, p + 4, "http")) {
    DLOG(INFO) << "missing status line; assuming HTTP/0.9";
    // Morph this into HTTP/1.0 since HTTP/0.9 has no status line.
    raw_headers_ = "HTTP/1.0";
    return;
  }

  p += 4;

  if (p >= line_end || *p != '/') {
    DLOG(INFO) << "missing version; assuming HTTP/1.0";
    raw_headers_ = "HTTP/1.0";
    return;
  }

  string::const_iterator dot = find(p, line_end, '.');
  if (dot == line_end) {
    DLOG(INFO) << "malformed version; assuming HTTP/1.0";
    raw_headers_ = "HTTP/1.0";
    return;
  }

  ++p;  // from / to first digit.
  ++dot;  // from . to second digit.

  if (!(*p >= '0' && *p <= '9' && *dot >= '0' && *dot <= '9')) {
    DLOG(INFO) << "malformed version number; assuming HTTP/1.0";
    raw_headers_ = "HTTP/1.0";
    return;
  }

  int major = *p - '0';
  int minor = *dot - '0';

  if ((major > 1) || ((major == 1) && (minor >= 1))) {
    // at least HTTP/1.1
    raw_headers_ = "HTTP/1.1";
  } else {
    // treat anything else as version 1.0
    raw_headers_ = "HTTP/1.0";
  }
}

// Note: this implementation implicitly assumes that line_end points at a valid
// sentinel character (such as '\0').
void HttpResponseHeaders::ParseStatusLine(string::const_iterator line_begin,
                                          string::const_iterator line_end) {
  ParseVersion(line_begin, line_end);

  string::const_iterator p = find(line_begin, line_end, ' ');

  if (p == line_end) {
    DLOG(INFO) << "missing response status; assuming 200 OK";
    raw_headers_.append(" 200 OK");
    raw_headers_.push_back('\0');
    response_code_ = 200;
    return;
  }

  // Skip whitespace.
  while (*p == ' ')
    ++p;

  string::const_iterator code = p;
  while (*p >= '0' && *p <= '9')
    ++p;

  if (p == code) {
    DLOG(INFO) << "missing response status number; assuming 200";
    raw_headers_.append(" 200 ");
    response_code_ = 200;
  } else {
    raw_headers_.push_back(' ');
    raw_headers_.append(code, p);
    raw_headers_.push_back(' ');
    response_code_ = static_cast<int>(StringToInt64(string(code, p)));
  }

  // Skip whitespace.
  while (*p == ' ')
    ++p;

  // Trim trailing whitespace.
  while (line_end > p && line_end[-1] == ' ')
    --line_end;

  if (p == line_end) {
    DLOG(INFO) << "missing response status text; assuming OK";
    raw_headers_.append("OK");
  } else {
    raw_headers_.append(p, line_end);
  }

  raw_headers_.push_back('\0');
}

size_t HttpResponseHeaders::FindHeader(size_t from,
                                       const string& search) const {
  for (size_t i = from; i < parsed_.size(); ++i) {
    if (parsed_[i].is_continuation())
      continue;
    const string::const_iterator& name_begin = parsed_[i].name_begin;
    const string::const_iterator& name_end = parsed_[i].name_end;
    if (static_cast<size_t>(name_end - name_begin) == search.size() &&
        std::equal(name_begin, name_end, search.begin(),
                   CaseInsensitiveCompare<char>()))
      return i;
  }

  return string::npos;
}

void HttpResponseHeaders::AddHeader(string::const_iterator name_begin,
                                    string::const_iterator name_end,
                                    string::const_iterator values_begin,
                                    string::const_iterator values_end) {
  // If the header can be coalesced, then we should split it up.
  if (values_begin == values_end ||
      HttpUtil::IsNonCoalescingHeader(name_begin, name_end)) {
    AddToParsed(name_begin, name_end, values_begin, values_end);
  } else {
    HttpUtil::ValuesIterator it(values_begin, values_end, ',');
    while (it.GetNext()) {
      AddToParsed(name_begin, name_end, it.value_begin(), it.value_end());
      // clobber these so that subsequent values are treated as continuations
      name_begin = name_end = raw_headers_.end();
    }
  }
}

void HttpResponseHeaders::AddToParsed(string::const_iterator name_begin,
                                      string::const_iterator name_end,
                                      string::const_iterator value_begin,
                                      string::const_iterator value_end) {
  ParsedHeader header;
  header.name_begin = name_begin;
  header.name_end = name_end;
  header.value_begin = value_begin;
  header.value_end = value_end;
  parsed_.push_back(header);
}

void HttpResponseHeaders::GetTransientHeaders(HeaderSet* result) const {
  // Add server specified transients.  Any 'cache-control: no-cache="foo,bar"'
  // headers present in the response specify additional headers that we should
  // not store in the cache.
  const string kCacheControl = "cache-control";
  const string kPrefix = "no-cache=\"";
  string value;
  void* iter = NULL;
  while (EnumerateHeader(&iter, kCacheControl, &value)) {
    if (value.size() > kPrefix.size() &&
        value.compare(0, kPrefix.size(), kPrefix) == 0) {
      // if it doesn't end with a quote, then treat as malformed
      if (value[value.size()-1] != '\"')
        continue;

      // trim off leading and trailing bits
      size_t len = value.size() - kPrefix.size() - 1;
      TrimString(value.substr(kPrefix.size(), len), HTTP_LWS, &value);

      size_t begin_pos = 0;
      for (;;) {
        // find the end of this header name
        size_t comma_pos = value.find(',', begin_pos);
        if (comma_pos == string::npos)
          comma_pos = value.size();
        size_t end = comma_pos;
        while (end > begin_pos && strchr(HTTP_LWS, value[end - 1]))
          end--;

        // assuming the header is not emtpy, lowercase and insert into set
        if (end > begin_pos) {
          string name = value.substr(begin_pos, end - begin_pos);
          StringToLowerASCII(&name);
          result->insert(name);
        }

        // repeat
        begin_pos = comma_pos + 1;
        while (begin_pos < value.size() && strchr(HTTP_LWS, value[begin_pos]))
          begin_pos++;
        if (begin_pos >= value.size())
          break;
      }
    }
  }

  // Add standard transient headers.  Perhaps we should move this to a
  // statically cached hash_set to avoid duplicated work?
  for (size_t i = 0; i < arraysize(kTransientHeaders); ++i)
    result->insert(string(kTransientHeaders[i]));
}

void HttpResponseHeaders::GetMimeTypeAndCharset(string* mime_type,
                                                string* charset) const {
  mime_type->clear();
  charset->clear();

  string name = "content-type";
  string value;

  bool had_charset = false;

  void* iter = NULL;
  while (EnumerateHeader(&iter, name, &value))
    HttpUtil::ParseContentType(value, mime_type, charset, &had_charset);
}

bool HttpResponseHeaders::GetMimeType(string* mime_type) const {
  string unused;
  GetMimeTypeAndCharset(mime_type, &unused);
  return !mime_type->empty();
}

bool HttpResponseHeaders::GetCharset(string* charset) const {
  string unused;
  GetMimeTypeAndCharset(&unused, charset);
  return !charset->empty();
}

bool HttpResponseHeaders::IsRedirect(string* location) const {
  // Users probably want to see 300 (multiple choice) pages, so we don't count
  // them as redirects that need to be followed.
  if (!(response_code_ == 301 ||
        response_code_ == 302 ||
        response_code_ == 303 ||
        response_code_ == 307))
    return false;

  // If we lack a Location header, then we can't treat this as a redirect.
  // We assume that the first non-empty location value is the target URL that
  // we want to follow.  TODO(darin): Is this consistent with other browsers?
  size_t i = -1;
  do {
    i = FindHeader(++i, "location");
    if (i == string::npos)
      return false;
    // If the location value is empty, then it doesn't count.
  } while (parsed_[i].value_begin == parsed_[i].value_end);

  if (location) {
    // Escape any non-ASCII characters to preserve them.  The server should
    // only be returning ASCII here, but for compat we need to do this.
    *location = EscapeNonASCII(
        std::string(parsed_[i].value_begin, parsed_[i].value_end));
  }

  return true;
}

// From RFC 2616 section 13.2.4:
//
// The calculation to determine if a response has expired is quite simple:
//
//   response_is_fresh = (freshness_lifetime > current_age)
//
// Of course, there are other factors that can force a response to always be
// validated or re-fetched.
//
bool HttpResponseHeaders::RequiresValidation(const Time& request_time,
                                             const Time& response_time,
                                             const Time& current_time) const {

  TimeDelta lifetime =
      GetFreshnessLifetime(response_time);
  if (lifetime == TimeDelta())
    return true;

  return lifetime <= GetCurrentAge(request_time, response_time, current_time);
}

// From RFC 2616 section 13.2.4:
//
// The max-age directive takes priority over Expires, so if max-age is present
// in a response, the calculation is simply:
//
//   freshness_lifetime = max_age_value
//
// Otherwise, if Expires is present in the response, the calculation is:
//
//   freshness_lifetime = expires_value - date_value
//
// Note that neither of these calculations is vulnerable to clock skew, since
// all of the information comes from the origin server.
//
// Also, if the response does have a Last-Modified time, the heuristic
// expiration value SHOULD be no more than some fraction of the interval since
// that time. A typical setting of this fraction might be 10%:
//
//   freshness_lifetime = (date_value - last_modified_value) * 0.10
//
TimeDelta HttpResponseHeaders::GetFreshnessLifetime(
    const Time& response_time) const {
  // Check for headers that force a response to never be fresh.  For backwards
  // compat, we treat "Pragma: no-cache" as a synonym for "Cache-Control:
  // no-cache" even though RFC 2616 does not specify it.
  if (HasHeaderValue("cache-control", "no-cache") ||
      HasHeaderValue("cache-control", "no-store") ||
      HasHeaderValue("pragma", "no-cache") ||
      HasHeaderValue("vary", "*"))  // see RFC 2616 section 13.6
    return TimeDelta();  // not fresh

  // NOTE: "Cache-Control: max-age" overrides Expires, so we only check the
  // Expires header after checking for max-age in GetFreshnessLifetime.  This
  // is important since "Expires: <date in the past>" means not fresh, but
  // it should not trump a max-age value.

  TimeDelta max_age_value;
  if (GetMaxAgeValue(&max_age_value))
    return max_age_value;

  // If there is no Date header, then assume that the server response was
  // generated at the time when we received the response.
  Time date_value;
  if (!GetDateValue(&date_value))
    date_value = response_time;

  Time expires_value;
  if (GetExpiresValue(&expires_value)) {
    // The expires value can be a date in the past!
    if (expires_value > date_value)
      return expires_value - date_value;

    return TimeDelta();  // not fresh
  }

  // From RFC 2616 section 13.4:
  //
  //   A response received with a status code of 200, 203, 206, 300, 301 or 410
  //   MAY be stored by a cache and used in reply to a subsequent request,
  //   subject to the expiration mechanism, unless a cache-control directive
  //   prohibits caching.
  //   ...
  //   A response received with any other status code (e.g. status codes 302
  //   and 307) MUST NOT be returned in a reply to a subsequent request unless
  //   there are cache-control directives or another header(s) that explicitly
  //   allow it.
  //
  // Since we do not support byte range requests yet, we exclude 206.  See
  // HttpCache::Transaction::ShouldPassThrough.
  //
  // From RFC 2616 section 14.9.4:
  //
  //   When the must-revalidate directive is present in a response received by
  //   a cache, that cache MUST NOT use the entry after it becomes stale to
  //   respond to a subsequent request without first revalidating it with the
  //   origin server. (I.e., the cache MUST do an end-to-end revalidation every
  //   time, if, based solely on the origin server's Expires or max-age value,
  //   the cached response is stale.)
  //
  if ((response_code_ == 200 || response_code_ == 203) &&
      !HasHeaderValue("cache-control", "must-revalidate")) {
    // TODO(darin): Implement a smarter heuristic.
    Time last_modified_value;
    if (GetLastModifiedValue(&last_modified_value)) {
      // The last-modified value can be a date in the past!
      if (last_modified_value <= date_value)
        return (date_value - last_modified_value) / 10;
    }
  }

  // These responses are implicitly fresh (unless otherwise overruled):
  if (response_code_ == 300 || response_code_ == 301 || response_code_ == 410)
    return TimeDelta::FromMicroseconds(kint64max);

  return TimeDelta();  // not fresh
}

// From RFC 2616 section 13.2.3:
//
// Summary of age calculation algorithm, when a cache receives a response:
//
//   /*
//    * age_value
//    *      is the value of Age: header received by the cache with
//    *              this response.
//    * date_value
//    *      is the value of the origin server's Date: header
//    * request_time
//    *      is the (local) time when the cache made the request
//    *              that resulted in this cached response
//    * response_time
//    *      is the (local) time when the cache received the
//    *              response
//    * now
//    *      is the current (local) time
//    */
//   apparent_age = max(0, response_time - date_value);
//   corrected_received_age = max(apparent_age, age_value);
//   response_delay = response_time - request_time;
//   corrected_initial_age = corrected_received_age + response_delay;
//   resident_time = now - response_time;
//   current_age   = corrected_initial_age + resident_time;
//
TimeDelta HttpResponseHeaders::GetCurrentAge(const Time& request_time,
                                             const Time& response_time,
                                             const Time& current_time) const {
  // If there is no Date header, then assume that the server response was
  // generated at the time when we received the response.
  Time date_value;
  if (!GetDateValue(&date_value))
    date_value = response_time;

  // If there is no Age header, then assume age is zero.  GetAgeValue does not
  // modify its out param if the value does not exist.
  TimeDelta age_value;
  GetAgeValue(&age_value);

  TimeDelta apparent_age = std::max(TimeDelta(), response_time - date_value);
  TimeDelta corrected_received_age = std::max(apparent_age, age_value);
  TimeDelta response_delay = response_time - request_time;
  TimeDelta corrected_initial_age = corrected_received_age + response_delay;
  TimeDelta resident_time = current_time - response_time;
  TimeDelta current_age = corrected_initial_age + resident_time;

  return current_age;
}

bool HttpResponseHeaders::GetMaxAgeValue(TimeDelta* result) const {
  string name = "cache-control";
  string value;

  const char kMaxAgePrefix[] = "max-age=";
  const size_t kMaxAgePrefixLen = arraysize(kMaxAgePrefix) - 1;

  void* iter = NULL;
  while (EnumerateHeader(&iter, name, &value)) {
    if (value.size() > kMaxAgePrefixLen) {
      if (LowerCaseEqualsASCII(value.begin(),
                               value.begin() + kMaxAgePrefixLen,
                               kMaxAgePrefix)) {
        *result = TimeDelta::FromSeconds(
            StringToInt64(value.substr(kMaxAgePrefixLen)));
        return true;
      }
    }
  }

  return false;
}

bool HttpResponseHeaders::GetAgeValue(TimeDelta* result) const {
  string value;
  if (!EnumerateHeader(NULL, "Age", &value))
    return false;

  *result = TimeDelta::FromSeconds(StringToInt64(value));
  return true;
}

bool HttpResponseHeaders::GetDateValue(Time* result) const {
  return GetTimeValuedHeader("Date", result);
}

bool HttpResponseHeaders::GetLastModifiedValue(Time* result) const {
  return GetTimeValuedHeader("Last-Modified", result);
}

bool HttpResponseHeaders::GetExpiresValue(Time* result) const {
  return GetTimeValuedHeader("Expires", result);
}

bool HttpResponseHeaders::GetTimeValuedHeader(const std::string& name,
                                              Time* result) const {
  string value;
  if (!EnumerateHeader(NULL, name, &value))
    return false;

  std::wstring value_wide(value.begin(), value.end());  // inflate ascii
  return Time::FromString(value_wide.c_str(), result);
}

bool HttpResponseHeaders::IsKeepAlive() const {
  const char kPrefix[] = "HTTP/1.0";
  const size_t kPrefixLen = arraysize(kPrefix) - 1;
  if (raw_headers_.size() < kPrefixLen)  // Lacking a status line?
    return false;

  // NOTE: It is perhaps risky to assume that a Proxy-Connection header is
  // meaningful when we don't know that this response was from a proxy, but
  // Mozilla also does this, so we'll do the same.
  string connection_val;
  void* iter = NULL;
  if (!EnumerateHeader(&iter, "connection", &connection_val))
    EnumerateHeader(&iter, "proxy-connection", &connection_val);

  bool keep_alive;

  if (std::equal(raw_headers_.begin(),
                 raw_headers_.begin() + kPrefixLen, kPrefix)) {
    // HTTP/1.0 responses default to NOT keep-alive
    keep_alive = LowerCaseEqualsASCII(connection_val, "keep-alive");
  } else {
    // HTTP/1.1 responses default to keep-alive
    keep_alive = !LowerCaseEqualsASCII(connection_val, "close");
  }

  return keep_alive;
}

// From RFC 2616:
// Content-Length = "Content-Length" ":" 1*DIGIT
int64 HttpResponseHeaders::GetContentLength() const {
  void* iter = NULL;
  string content_length_val;
  if (!EnumerateHeader(&iter, "content-length", &content_length_val))
    return -1;

  if (content_length_val.empty())
    return -1;

  if (content_length_val[0] == '+')
    return -1;

  int64 result;
  bool ok = StringToInt64(content_length_val, &result);
  if (!ok || result < 0) 
    return -1;

  return result;
}

}  // namespace net
