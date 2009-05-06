// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/fetch/http_session.h"
#include "net/tools/fetch/http_server_response_info.h"

HttpSession::HttpSession(const std::string& ip, int port)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_(HttpListenSocket::Listen(ip, port, this))) {
}

HttpSession::~HttpSession() {
}

void HttpSession::OnRequest(HttpListenSocket* connection, 
                            HttpServerRequestInfo* info) {
  // TODO(mbelshe):  Make this function more interesting.

  // Generate a 10KB sequence of data.
  static std::string data;
  if (data.length() == 0) {
    while (data.length() < (10 * 1024))
      data += 'a' + (rand() % 26);
  }

  HttpServerResponseInfo response_info;
  response_info.protocol = "HTTP/1.1";
  response_info.status = 200;
  response_info.content_type = "text/plain";
  response_info.content_length = data.length();

  connection->Respond(&response_info, data);
}
