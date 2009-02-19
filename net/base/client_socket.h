// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CLIENT_SOCKET_H_
#define NET_BASE_CLIENT_SOCKET_H_

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <sys/socket.h>
#endif

#include "net/base/socket.h"

namespace net {

class ClientSocket : public Socket {
 public:
  // Called to establish a connection.  Returns OK if the connection could be
  // established synchronously.  Otherwise, ERR_IO_PENDING is returned and the
  // given callback will run asynchronously when the connection is established
  // or when an error occurs.  The result is some other error code if the
  // connection could not be established.
  //
  // The socket's Read and Write methods may not be called until Connect
  // succeeds.
  //
  // It is valid to call Connect on an already connected socket, in which case
  // OK is simply returned.
  //
  // Connect may also be called again after a call to the Disconnect method.
  //
  virtual int Connect(CompletionCallback* callback) = 0;

  // If a non-fatal error occurs during Connect, the consumer can call this
  // method to re-Connect ignoring the error that occured.  This call is only
  // valid for certain errors.
  virtual int ReconnectIgnoringLastError(CompletionCallback* callback) = 0;

  // Called to disconnect a connected socket.  Does nothing if the socket is
  // already disconnected.  After calling Disconnect it is possible to call
  // Connect again to establish a new connection.
  virtual void Disconnect() = 0;

  // Called to test if the connection is still alive.  Returns false if a
  // connection wasn't established or the connection is dead.
  virtual bool IsConnected() const = 0;

  // Called to test if the connection is still alive and idle.  Returns false
  // if a connection wasn't established, the connection is dead, or some data
  // have been received.
  virtual bool IsConnectedAndIdle() const = 0;

#if defined(OS_LINUX)
  // Identical to posix system call getpeername().
  // Needed by ssl_client_socket_nss.
  virtual int GetPeerName(struct sockaddr *name, socklen_t *namelen);
#endif
};

}  // namespace net

#endif  // NET_BASE_CLIENT_SOCKET_H_

