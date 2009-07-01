// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_pool.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/tcp_client_socket.h"

using base::TimeDelta;

namespace net {

TCPConnectJob::TCPConnectJob(
    const std::string& group_name,
    const HostResolver::RequestInfo& resolve_info,
    const ClientSocketHandle* handle,
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    Delegate* delegate)
    : ConnectJob(group_name, handle, delegate),
      resolve_info_(resolve_info),
      client_socket_factory_(client_socket_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this,
                    &TCPConnectJob::OnIOComplete)),
      resolver_(host_resolver) {}

TCPConnectJob::~TCPConnectJob() {
  // We don't worry about cancelling the host resolution and TCP connect, since
  // ~SingleRequestHostResolver and ~ClientSocket will take care of it.
}

int TCPConnectJob::Connect() {
  next_state_ = kStateResolveHost;
  return DoLoop(OK);
}

void TCPConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    delegate()->OnConnectJobComplete(rv, this);  // Deletes |this|
}

int TCPConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, kStateNone);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = kStateNone;
    switch (state) {
      case kStateResolveHost:
        DCHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case kStateResolveHostComplete:
        rv = DoResolveHostComplete(rv);
        break;
      case kStateTCPConnect:
        DCHECK_EQ(OK, rv);
        rv = DoTCPConnect();
        break;
      case kStateTCPConnectComplete:
        rv = DoTCPConnectComplete(rv);
        break;
      default:
        NOTREACHED();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != kStateNone);

  return rv;
}

int TCPConnectJob::DoResolveHost() {
  set_load_state(LOAD_STATE_RESOLVING_HOST);
  next_state_ = kStateResolveHostComplete;
  return resolver_.Resolve(resolve_info_, &addresses_, &callback_);
}

int TCPConnectJob::DoResolveHostComplete(int result) {
  DCHECK_EQ(LOAD_STATE_RESOLVING_HOST, load_state());
  if (result == OK)
    next_state_ = kStateTCPConnect;
  return result;
}

int TCPConnectJob::DoTCPConnect() {
  next_state_ = kStateTCPConnectComplete;
  set_load_state(LOAD_STATE_CONNECTING);
  set_socket(client_socket_factory_->CreateTCPClientSocket(addresses_));
  connect_start_time_ = base::TimeTicks::Now();
  return socket()->Connect(&callback_);
}

int TCPConnectJob::DoTCPConnectComplete(int result) {
  DCHECK_EQ(load_state(), LOAD_STATE_CONNECTING);
  if (result == OK) {
    DCHECK(connect_start_time_ != base::TimeTicks());
    base::TimeDelta connect_duration =
        base::TimeTicks::Now() - connect_start_time_;

    UMA_HISTOGRAM_CLIPPED_TIMES("Net.TCP_Connection_Latency",
        connect_duration,
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10),
        100);
  }

  return result;
}

ConnectJob* TCPClientSocketPool::TCPConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const ClientSocketPoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return new TCPConnectJob(
      group_name, request.resolve_info, request.handle,
      client_socket_factory_, host_resolver_, delegate);
}

TCPClientSocketPool::TCPClientSocketPool(
    int max_sockets_per_group,
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory)
    : base_(new ClientSocketPoolBase(
        max_sockets_per_group,
        new TCPConnectJobFactory(client_socket_factory, host_resolver))) {}

TCPClientSocketPool::~TCPClientSocketPool() {}

int TCPClientSocketPool::RequestSocket(
    const std::string& group_name,
    const HostResolver::RequestInfo& resolve_info,
    int priority,
    ClientSocketHandle* handle,
    CompletionCallback* callback) {
  return base_->RequestSocket(
      group_name, resolve_info, priority, handle, callback);
}

void TCPClientSocketPool::CancelRequest(
    const std::string& group_name,
    const ClientSocketHandle* handle) {
  base_->CancelRequest(group_name, handle);
}

void TCPClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    ClientSocket* socket) {
  base_->ReleaseSocket(group_name, socket);
}

void TCPClientSocketPool::CloseIdleSockets() {
  base_->CloseIdleSockets();
}

int TCPClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_->IdleSocketCountInGroup(group_name);
}

LoadState TCPClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_->GetLoadState(group_name, handle);
}

}  // namespace net
