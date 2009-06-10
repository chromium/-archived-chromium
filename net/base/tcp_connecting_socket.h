// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_CONNECTING_SOCKET_H_
#define NET_BASE_TCP_CONNECTING_SOCKET_H_

#include <deque>
#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "net/base/address_list.h"
#include "net/base/client_socket_pool.h"
#include "net/base/host_resolver.h"

namespace net {

class ClientSocketFactory;
class ClientSocketPoolBase;

// TCPConnectingSocket handles host resolution and socket connection for a TCP
// socket.
class TCPConnectingSocket {
 public:
  TCPConnectingSocket(const std::string& group_name,
                      const std::string& host,
                      int port,
                      const ClientSocketHandle* handle,
                      CompletionCallback* callback,
                      ClientSocketFactory* client_socket_factory,
                      const scoped_refptr<ClientSocketPoolBase>& pool);
  ~TCPConnectingSocket();

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int Connect();

  // TODO(willchan): Delete this function once we decouple connecting sockets
  // from requests, since we'll keep around the idle connected socket.
  // Called by the ClientSocketPoolBase to cancel this TCPConnectingSocket.
  // Only necessary if a ClientSocketHandle is reused.
  void Cancel();

 private:
  // Handles asynchronous completion of IO.  |result| represents the result of
  // the IO operation.
  void OnIOComplete(int result);

  // Handles both asynchronous and synchronous completion of IO.  |result|
  // represents the result of the IO operation.  |synchronous| indicates
  // whether or not the previous IO operation completed synchronously or
  // asynchronously.  OnIOCompleteInternal returns the result of the next IO
  // operation that executes, or just the value of |result|.
  int OnIOCompleteInternal(int result, bool synchronous);

  const std::string group_name_;
  const std::string host_;
  const int port_;
  const ClientSocketHandle* const handle_;
  CompletionCallback* const user_callback_;
  ClientSocketFactory* const client_socket_factory_;
  CompletionCallbackImpl<TCPConnectingSocket> callback_;
  scoped_ptr<ClientSocket> socket_;
  const scoped_refptr<ClientSocketPoolBase> pool_;
  HostResolver resolver_;
  AddressList addresses_;
  bool canceled_;

  // The time the Connect() method was called (if it got called).
  base::Time connect_start_time_;

  DISALLOW_COPY_AND_ASSIGN(TCPConnectingSocket);
};

}  // namespace net

#endif  // NET_BASE_TCP_CONNECTING_SOCKET_H_
