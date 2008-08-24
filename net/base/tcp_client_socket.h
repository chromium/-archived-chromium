// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_CLIENT_SOCKET_H_
#define NET_BASE_TCP_CLIENT_SOCKET_H_

#include <ws2tcpip.h>

#include "base/object_watcher.h"
#include "net/base/address_list.h"
#include "net/base/client_socket.h"

namespace net {

// A client socket that uses TCP as the transport layer.
//
// NOTE: The implementation supports half duplex only.  Read and Write calls
// must not be in progress at the same time.
class TCPClientSocket : public ClientSocket,
                        public base::ObjectWatcher::Delegate {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  explicit TCPClientSocket(const AddressList& addresses);

  ~TCPClientSocket();

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual int ReconnectIgnoringLastError(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;

  // Socket methods:
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(const char* buf, int buf_len, CompletionCallback* callback);

 private:
  int CreateSocket(const struct addrinfo* ai);
  void DoCallback(int rv);
  void DidCompleteConnect();
  void DidCompleteIO();

  // base::ObjectWatcher::Delegate methods:
  virtual void OnObjectSignaled(HANDLE object);

  SOCKET socket_;
  OVERLAPPED overlapped_;
  WSABUF buffer_;

  base::ObjectWatcher watcher_;

  CompletionCallback* callback_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // The addrinfo that we are attempting to use or NULL if all addrinfos have
  // been tried.
  const struct addrinfo* current_ai_;

  enum WaitState {
    NOT_WAITING,
    WAITING_CONNECT,
    WAITING_READ,
    WAITING_WRITE
  };
  WaitState wait_state_;
};

}  // namespace net

#endif  // NET_BASE_TCP_CLIENT_SOCKET_H_

