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

#include "net/http/http_vary_data.h"

#include <stdlib.h>

#include "base/pickle.h"
#include "base/string_util.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

HttpVaryData::HttpVaryData() : is_valid_(false) {
  memset(&request_digest_, 0, sizeof(request_digest_));
}

bool HttpVaryData::Init(const HttpRequestInfo& request_info,
                        const HttpResponseHeaders& response_headers) {
  MD5Context ctx;
  MD5Init(&ctx);

  bool processed_header = false;

  // Feed the MD5 context in the order of the Vary header enumeration.  If the
  // Vary header repeats a header name, then that's OK.
  //
  // If the Vary header contains '*' then we should not construct any vary data
  // since it is all usurped by a '*'.  See section 13.6 of RFC 2616.
  //
  void* iter = NULL;
  std::string name = "vary", request_header;
  while (response_headers.EnumerateHeader(&iter, name, &request_header)) {
    if (request_header == "*")
      return false;
    AddField(request_info, request_header, &ctx);
    processed_header = true;
  }

  // Add an implicit 'Vary: cookie' header to any redirect to avoid redirect
  // loops which may result from redirects that are incorrectly marked as
  // cachable by the server.  Unfortunately, other browsers do not cache
  // redirects that result from requests containing a cookie header.  We are
  // treading on untested waters here, so we want to be extra careful to make
  // sure we do not end up with a redirect loop served from cache.
  //
  // If there is an explicit 'Vary: cookie' header, then we will just end up
  // digesting the cookie header twice.  Not a problem.
  //
  std::string location;
  if (response_headers.IsRedirect(&location)) {
    AddField(request_info, "cookie", &ctx);
    processed_header = true;
  }

  if (!processed_header)
    return false;

  MD5Final(&request_digest_, &ctx);
  return is_valid_ = true;
}

bool HttpVaryData::InitFromPickle(const Pickle& pickle, void** iter) {
  const char* data;
  if (pickle.ReadBytes(iter, &data, sizeof(request_digest_))) {
    memcpy(&request_digest_, data, sizeof(request_digest_));
    return is_valid_ = true;
  }
  return false;
}

void HttpVaryData::Persist(Pickle* pickle) const {
  pickle->WriteBytes(&request_digest_, sizeof(request_digest_));
}

bool HttpVaryData::MatchesRequest(
    const HttpRequestInfo& request_info,
    const HttpResponseHeaders& cached_response_headers) const {
  HttpVaryData new_vary_data;
  if (!new_vary_data.Init(request_info, cached_response_headers)) {
    // This shouldn't happen provided the same response headers passed here
    // were also used when initializing |this|.
    NOTREACHED();
    return false;
  }
  return memcmp(&new_vary_data.request_digest_, &request_digest_,
                sizeof(request_digest_)) == 0;
}

// static
std::string HttpVaryData::GetRequestValue(
    const HttpRequestInfo& request_info,
    const std::string& request_header) {
  // Some special cases:
  if (LowerCaseEqualsASCII(request_header, "referer"))
    return request_info.referrer.spec();
  if (LowerCaseEqualsASCII(request_header, "user-agent"))
    return request_info.user_agent;

  std::string result;

  // Check extra headers:
  HttpUtil::HeadersIterator it(request_info.extra_headers.begin(),
                               request_info.extra_headers.end(),
                               "\r\n");
  while (it.GetNext()) {
    size_t name_len = it.name_end() - it.name_begin();
    if (request_header.size() == name_len &&
        std::equal(it.name_begin(), it.name_end(), request_header.begin(),
                   CaseInsensitiveCompare<char>())) {
      if (!result.empty())
        result.append(1, ',');
      result.append(it.values());
    }
  }

  // Unfortunately, we do not have access to all of the request headers at this
  // point.  Most notably, we do not have access to an Authorization header if
  // one will be added to the request.

  return result;
}

// static
void HttpVaryData::AddField(const HttpRequestInfo& request_info,
                            const std::string& request_header,
                            MD5Context* ctx) {
  std::string request_value = GetRequestValue(request_info, request_header);

  // Append a character that cannot appear in the request header line so that we
  // protect against case where the concatenation of two request headers could
  // look the same for a variety of values for the individual request headers.
  // For example, "foo: 12\nbar: 3" looks like "foo: 1\nbar: 23" otherwise.
  request_value.append(1, '\n');

  MD5Update(ctx, request_value.data(), request_value.size());
}

}  // namespace net
