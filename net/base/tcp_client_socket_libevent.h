// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_CLIENT_SOCKET_LIBEVENT_H_
#define NET_BASE_TCP_CLIENT_SOCKET_LIBEVENT_H_

#include <sys/socket.h>  // for struct sockaddr

#include "base/message_loop.h"
#include "net/base/address_list.h"
#include "net/base/client_socket.h"
#include "net/base/completion_callback.h"

struct event;  // From libevent

namespace net {

// A client socket that uses TCP as the transport layer.
class TCPClientSocketLibevent : public ClientSocket,
                                public MessageLoopForIO::Watcher {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  explicit TCPClientSocketLibevent(const AddressList& addresses);

  ~TCPClientSocketLibevent();

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);

  // Identical to posix system call of same name
  // Needed by ssl_client_socket_nss
  virtual int GetPeerName(struct sockaddr *name, socklen_t *namelen);

 private:
  // Called by MessagePumpLibevent when the socket is ready to do I/O
  void OnFileCanReadWithoutBlocking(int fd);
  void OnFileCanWriteWithoutBlocking(int fd);

  void DoReadCallback(int rv);
  void DoWriteCallback(int rv);
  void DidCompleteRead();
  void DidCompleteWrite();
  void DidCompleteConnect();

  int CreateSocket(const struct addrinfo* ai);

  int socket_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list, or NULL if all addrinfos have been tried.
  const struct addrinfo* current_ai_;

  // Whether we're currently waiting for connect() to complete
  bool waiting_connect_;

  // The socket's libevent wrapper
  MessageLoopForIO::FileDescriptorWatcher socket_watcher_;

  // The buffer used by OnSocketReady to retry Read requests
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;

  // The buffer used by OnSocketReady to retry Write requests
  scoped_refptr<IOBuffer> write_buf_;
  int write_buf_len_;

  // External callback; called when read is complete.
  CompletionCallback* read_callback_;

  // External callback; called when write is complete.
  CompletionCallback* write_callback_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketLibevent);
};

}  // namespace net

#endif  // NET_BASE_TCP_CLIENT_SOCKET_LIBEVENT_H_
