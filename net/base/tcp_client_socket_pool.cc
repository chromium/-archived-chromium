// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/tcp_client_socket_pool.h"

#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/stl_util-inl.h"
#include "net/base/client_socket_factory.h"
#include "net/base/client_socket_handle.h"
#include "net/base/dns_resolution_observer.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/tcp_connecting_socket.h"

using base::TimeDelta;

namespace net {

TCPClientSocketPool::TCPClientSocketPool(
    int max_sockets_per_group,
    ClientSocketFactory* client_socket_factory)
    : base_(new ClientSocketPoolBase(max_sockets_per_group,
                                     &connecting_socket_factory_)),
      connecting_socket_factory_(base_, client_socket_factory) {}

TCPClientSocketPool::~TCPClientSocketPool() {}

int TCPClientSocketPool::RequestSocket(const std::string& group_name,
                                       const std::string& host,
                                       int port,
                                       int priority,
                                       ClientSocketHandle* handle,
                                       CompletionCallback* callback) {
  ClientSocketPoolBase::Request request;
  request.host = host;
  request.port = port;
  request.priority = priority;
  request.handle = handle;
  request.callback = callback;
  request.load_state = LOAD_STATE_IDLE;
  return base_->RequestSocket(group_name, request);
}

void TCPClientSocketPool::CancelRequest(const std::string& group_name,
                                        const ClientSocketHandle* handle) {
  base_->CancelRequest(group_name, handle);
}

void TCPClientSocketPool::ReleaseSocket(const std::string& group_name,
                                        ClientSocket* socket) {
  base_->ReleaseSocket(group_name, socket);
}

void TCPClientSocketPool::CloseIdleSockets() {
  base_->CleanupIdleSockets(true);
}

int TCPClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_->IdleSocketCountInGroup(group_name);
}

LoadState TCPClientSocketPool::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  return base_->GetLoadState(group_name, handle);
}

TCPClientSocketPool::TCPConnectingSocketFactory::TCPConnectingSocketFactory(
    const scoped_refptr<ClientSocketPoolBase>& base,
    ClientSocketFactory* client_socket_factory)
    : base_(base),
      client_socket_factory_(client_socket_factory) {}

TCPClientSocketPool::TCPConnectingSocketFactory::~TCPConnectingSocketFactory() {
}

TCPConnectingSocket*
TCPClientSocketPool::TCPConnectingSocketFactory::CreateConnectingSocket(
    const std::string& group_name,
    const ClientSocketPoolBase::Request& request) const {
  return new TCPConnectingSocket(group_name,
                                 request.host,
                                 request.port,
                                 request.handle,
                                 request.callback,
                                 client_socket_factory_,
                                 base_);
}

}  // namespace net
