// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_INFO_H__
#define NET_HTTP_HTTP_RESPONSE_INFO_H__

#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/ssl_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_vary_data.h"

namespace net {

class HttpResponseInfo {
 public:
  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time;

  // If the response headers indicate a 401 or 407 failure, then this structure
  // will contain additional information about the authentication challenge.
  scoped_refptr<AuthChallengeInfo> auth_challenge;

  // The SSL connection info (if HTTPS).
  SSLInfo ssl_info;

  // The parsed response headers and status line.
  scoped_refptr<HttpResponseHeaders> headers;

  // The "Vary" header data for this response.
  HttpVaryData vary_data;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_INFO_H__

