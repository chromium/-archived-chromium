// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_INFO_H__
#define NET_HTTP_HTTP_RESPONSE_INFO_H__

#include <string>
#include <map>

// Meta information about a server response.
class HttpServerResponseInfo {
 public:
  HttpServerResponseInfo() 
      : status(200), content_length(0), connection_close(false) {
  }

  // The response protocol
  std::string protocol;

  // The status code
  int status;

  // The server identifier
  std::string server_name;

  // The content type
  std::string content_type;

  // The content length
  int content_length;

  // Should we close the connection
  bool connection_close;

  // Additional response headers
  std::map<std::string, std::string> headers;
};

#endif  // NET_HTTP_HTTP_RESPONSE_INFO_H__
