// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TOOLS_HTTP_SERVER_REQUEST_INFO_H_
#define NET_BASE_TOOLS_HTTP_SERVER_REQUEST_INFO_H_

#include <string>

#include "net/http/http_request_info.h"

// Meta information about an HTTP request.
// This is geared toward servers in that it keeps a map of the headers and
// values rather than just a list of header strings (which net::HttpRequestInfo
// does).
class HttpServerRequestInfo : public net::HttpRequestInfo {
 public:
  HttpServerRequestInfo() : net::HttpRequestInfo() {}

  // A map of the names -> values for HTTP headers.
  std::map<std::string, std::string> headers;
};

#endif  // NET_BASE_TOOLS_HTTP_SERVER_REQUEST_INFO_H_
