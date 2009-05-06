// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/fetch/http_server.h"
#include "net/tools/fetch/http_listen_socket.h"

HttpServer::HttpServer(std::string ip, int port)
    : ALLOW_THIS_IN_INITIALIZER_LIST(session_(new HttpSession(ip, port))) {
}

HttpServer::~HttpServer() {
}
