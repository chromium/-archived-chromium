// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CLIENT_SOCKET_H_
#define NET_BASE_SSL_CLIENT_SOCKET_H_

#include "net/base/client_socket.h"

namespace net {

class SSLInfo;

// A client socket that uses SSL as the transport layer.
//
// NOTE: The SSL handshake occurs within the Connect method after a TCP
// connection is established.  If a SSL error occurs during the handshake,
// Connect will fail.
//
class SSLClientSocket : public ClientSocket {
 public:
  // Gets the SSL connection information of the socket.
  virtual void GetSSLInfo(SSLInfo* ssl_info) = 0;
};

}  // namespace net

#endif  // NET_BASE_SSL_CLIENT_SOCKET_H_
