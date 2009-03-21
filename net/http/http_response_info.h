// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_INFO_H_
#define NET_HTTP_HTTP_RESPONSE_INFO_H_

#include "base/platform_file.h"
#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/ssl_info.h"
#include "net/http/http_vary_data.h"

namespace net {

class HttpResponseHeaders;

class HttpResponseInfo {
 public:
  HttpResponseInfo();
  ~HttpResponseInfo();
  // Default copy-constructor and assignment operator are OK!

  // The following is only defined if the request_time member is set.
  // If this response was resurrected from cache, then this bool is set, and
  // request_time may corresponds to a time "far" in the past.  Note that
  // stale content (perhaps un-cacheable) may be fetched from cache subject to
  // the load flags specified on the request info.  For example, this is done
  // when a user presses the back button to re-render pages, or at startup, when
  // reloading previously visited pages (without going over the network).
  bool was_cached;

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

  // Platform specific file handle to the response data, if response data is
  // not in a standalone file, its value is base::kInvalidPlatformFileValue.
  base::PlatformFile response_data_file;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_INFO_H_
