// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_POOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool.h"

namespace net {

class ClientSocketFactory;

// TCPConnectJob handles the host resolution necessary for socket creation
// and the tcp connect.
class TCPConnectJob : public ConnectJob {
 public:
  TCPConnectJob(const std::string& group_name,
                const HostResolver::RequestInfo& resolve_info,
                const ClientSocketHandle* handle,
                ClientSocketFactory* client_socket_factory,
                HostResolver* host_resolver,
                Delegate* delegate);
  virtual ~TCPConnectJob();

  // ConnectJob methods.

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  virtual int Connect();

 private:
  enum State {
    kStateResolveHost,
    kStateResolveHostComplete,
    kStateTCPConnect,
    kStateTCPConnectComplete,
    kStateNone,
  };

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoTCPConnect();
  int DoTCPConnectComplete(int result);

  const HostResolver::RequestInfo resolve_info_;
  ClientSocketFactory* const client_socket_factory_;
  CompletionCallbackImpl<TCPConnectJob> callback_;
  SingleRequestHostResolver resolver_;
  AddressList addresses_;
  State next_state_;

  // The time the Connect() method was called (if it got called).
  base::TimeTicks connect_start_time_;

  DISALLOW_COPY_AND_ASSIGN(TCPConnectJob);
};

class TCPClientSocketPool : public ClientSocketPool {
 public:
  TCPClientSocketPool(int max_sockets_per_group,
                      HostResolver* host_resolver,
                      ClientSocketFactory* client_socket_factory);

  // ClientSocketPool methods:

  virtual int RequestSocket(const std::string& group_name,
                            const HostResolver::RequestInfo& resolve_info,
                            int priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback);

  virtual void CancelRequest(const std::string& group_name,
                             const ClientSocketHandle* handle);

  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket);

  virtual void CloseIdleSockets();

  virtual int IdleSocketCount() const {
    return base_->idle_socket_count();
  }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const;

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const;

 private:
  virtual ~TCPClientSocketPool();

  class TCPConnectJobFactory
      : public ClientSocketPoolBase::ConnectJobFactory {
   public:
    TCPConnectJobFactory(ClientSocketFactory* client_socket_factory,
                         HostResolver* host_resolver)
        : client_socket_factory_(client_socket_factory),
          host_resolver_(host_resolver) {}

    virtual ~TCPConnectJobFactory() {}

    // ClientSocketPoolBase::ConnectJobFactory methods.

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const ClientSocketPoolBase::Request& request,
        ConnectJob::Delegate* delegate) const;

   private:
    ClientSocketFactory* const client_socket_factory_;
    const scoped_refptr<HostResolver> host_resolver_;

    DISALLOW_COPY_AND_ASSIGN(TCPConnectJobFactory);
  };

  // One might ask why ClientSocketPoolBase is also refcounted if its
  // containing ClientSocketPool is already refcounted.  The reason is because
  // DoReleaseSocket() posts a task.  If ClientSocketPool gets deleted between
  // the posting of the task and the execution, then we'll hit the DCHECK that
  // |ClientSocketPoolBase::group_map_| is empty.
  scoped_refptr<ClientSocketPoolBase> base_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_POOL_H_
