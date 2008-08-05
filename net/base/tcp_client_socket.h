// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

  // MessageLoop::Watcher methods:
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
