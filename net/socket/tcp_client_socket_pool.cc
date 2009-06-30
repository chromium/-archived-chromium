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
    : group_name_(group_name),
      resolve_info_(resolve_info),
      handle_(handle),
      client_socket_factory_(client_socket_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this,
                    &TCPConnectJob::OnIOComplete)),
      delegate_(delegate),
      resolver_(host_resolver) {}

TCPConnectJob::~TCPConnectJob() {
  // We don't worry about cancelling the host resolution and TCP connect, since
  // ~SingleRequestHostResolver and ~ClientSocket will take care of it.
}

int TCPConnectJob::Connect() {
  set_load_state(LOAD_STATE_RESOLVING_HOST);
  int rv = resolver_.Resolve(resolve_info_, &addresses_, &callback_);
  if (rv != ERR_IO_PENDING)
    rv = OnIOCompleteInternal(rv, true /* synchronous */);
  return rv;
}

void TCPConnectJob::OnIOComplete(int result) {
  OnIOCompleteInternal(result, false /* asynchronous */);
}

int TCPConnectJob::OnIOCompleteInternal(
    int result, bool synchronous) {
  CHECK(result != ERR_IO_PENDING);

  if (result == OK && load_state() == LOAD_STATE_RESOLVING_HOST) {
    set_load_state(LOAD_STATE_CONNECTING);
    socket_.reset(client_socket_factory_->CreateTCPClientSocket(addresses_));
    connect_start_time_ = base::TimeTicks::Now();
    result = socket_->Connect(&callback_);
    if (result == ERR_IO_PENDING)
      return result;
  }

  if (result == OK) {
    DCHECK_EQ(load_state(), LOAD_STATE_CONNECTING);
    CHECK(connect_start_time_ != base::TimeTicks());
    base::TimeDelta connect_duration =
        base::TimeTicks::Now() - connect_start_time_;

    UMA_HISTOGRAM_CLIPPED_TIMES("Net.TCP_Connection_Latency",
        connect_duration,
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10),
        100);
  }

  // Now, we either succeeded at Connect()'ing, or we failed at host resolution
  // or Connect()'ing.  Either way, we'll run the callback to alert the client.

  delegate_->OnConnectJobComplete(
      group_name_,
      handle_,
      result == OK ? socket_.release() : NULL,
      result,
      !synchronous);

  // |this| is deleted after this point.
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
