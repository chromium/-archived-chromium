// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TOOLS_HTTP_SESSION_H_
#define NET_BASE_TOOLS_HTTP_SESSION_H_

#include "base/basictypes.h"
#include "net/http/http_request_info.h"
#include "net/tools/fetch/http_listen_socket.h"

// An HttpSession encapsulates a server-side HTTP listen socket.
class HttpSession : HttpListenSocket::Delegate {
public:
  HttpSession(const std::string& ip, int port);
  virtual ~HttpSession();

  virtual void OnRequest(HttpListenSocket* connection, 
                         HttpServerRequestInfo* info);

private:
  scoped_ptr<HttpListenSocket> socket_;
  DISALLOW_EVIL_CONSTRUCTORS(HttpSession);
};

#endif // NET_BASE_TOOLS_HTTP_SESSION_H_

