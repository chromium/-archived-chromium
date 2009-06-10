// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_CLIENT_SOCKET_POOL_H_
#define NET_BASE_TCP_CLIENT_SOCKET_POOL_H_

#include <string>

#include "net/base/client_socket_pool.h"
#include "net/base/client_socket_pool_base.h"

namespace net {

class ClientSocketFactory;
class TCPConnectingSocket;

// A TCPClientSocketPool is used to restrict the number of TCP sockets open at
// a time.  It also maintains a list of idle persistent sockets.
//
class TCPClientSocketPool : public ClientSocketPool {
 public:
  TCPClientSocketPool(int max_sockets_per_group,
                      ClientSocketFactory* client_socket_factory);

  // ClientSocketPool methods:

  virtual int RequestSocket(const std::string& group_name,
                            const std::string& host,
                            int port,
                            int priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback);

  virtual void CancelRequest(const std::string& group_name,
                             const ClientSocketHandle* handle);

  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket);

  virtual void CloseIdleSockets();

  virtual int idle_socket_count() const { return base_->idle_socket_count(); }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const;

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const;

 private:
  class TCPConnectingSocketFactory
      : public ClientSocketPoolBase::ConnectingSocketFactory {
   public:
    TCPConnectingSocketFactory(
        const scoped_refptr<ClientSocketPoolBase>& base,
        ClientSocketFactory* factory);
    virtual ~TCPConnectingSocketFactory();

    virtual TCPConnectingSocket* CreateConnectingSocket(
        const std::string& group_name,
        const ClientSocketPoolBase::Request& request) const;

   private:
    const scoped_refptr<ClientSocketPoolBase> base_;
    ClientSocketFactory* const client_socket_factory_;

    DISALLOW_COPY_AND_ASSIGN(TCPConnectingSocketFactory);
  };

  virtual ~TCPClientSocketPool();

  const scoped_refptr<ClientSocketPoolBase> base_;
  const TCPConnectingSocketFactory connecting_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketPool);
};

}  // namespace net

#endif  // NET_BASE_TCP_CLIENT_SOCKET_POOL_H_
