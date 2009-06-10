// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/tcp_connecting_socket.h"

#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "net/base/client_socket_factory.h"
#include "net/base/client_socket_handle.h"
#include "net/base/client_socket_pool_base.h"
#include "net/base/dns_resolution_observer.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_client_socket.h"

using base::TimeDelta;

namespace {

// The timeout value, in seconds, used to clean up idle sockets that can't be
// reused.
//
// Note: It's important to close idle sockets that have received data as soon
// as possible because the received data may cause BSOD on Windows XP under
// some conditions.  See http://crbug.com/4606.
const int kCleanupInterval = 10;  // DO NOT INCREASE THIS TIMEOUT.

// The maximum duration, in seconds, to keep idle persistent sockets alive.
const int kIdleTimeout = 300;  // 5 minutes.

}  // namespace

namespace net {

TCPConnectingSocket::TCPConnectingSocket(
    const std::string& group_name,
    const std::string& host,
    int port,
    const ClientSocketHandle* handle,
    CompletionCallback* callback,
    ClientSocketFactory* client_socket_factory,
    const scoped_refptr<ClientSocketPoolBase>& pool)
    : group_name_(group_name),
      host_(host),
      port_(port),
      handle_(handle),
      user_callback_(callback),
      client_socket_factory_(client_socket_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this,
                    &TCPConnectingSocket::OnIOComplete)),
      pool_(pool),
      canceled_(false) {}

TCPConnectingSocket::~TCPConnectingSocket() {}

int TCPConnectingSocket::Connect() {
  DCHECK(!canceled_);
  DidStartDnsResolution(host_, this);
  int rv = resolver_.Resolve(host_, port_, &addresses_, &callback_);
  if (rv == OK) {
    rv = OnIOCompleteInternal(rv, true /* synchronous */);
  }
  return rv;
}

void TCPConnectingSocket::OnIOComplete(int result) {
  OnIOCompleteInternal(result, false /* asynchronous */);
}

int TCPConnectingSocket::OnIOCompleteInternal(int result, bool synchronous) {
  DCHECK_NE(result, ERR_IO_PENDING);

  if (canceled_) {
    // We got canceled, so bail out.
    delete this;
    return result;
  }

  ClientSocketPoolBase::Request* request = pool_->GetConnectingRequest(
      group_name_, handle_);
  if (!request) {
    // The request corresponding to this TCPConnectingSocket has been canceled.
    // Stop bothering with it.
    delete this;
    return result;
  }

  if (result == OK && request->load_state == LOAD_STATE_RESOLVING_HOST) {
    request->load_state = LOAD_STATE_CONNECTING;
    socket_.reset(client_socket_factory_->CreateTCPClientSocket(addresses_));
    connect_start_time_ = base::Time::Now();
    result = socket_->Connect(&callback_);
    if (result == ERR_IO_PENDING)
      return result;
  }

  if (result == OK) {
    DCHECK_EQ(request->load_state, LOAD_STATE_CONNECTING);
    DCHECK(connect_start_time_ != base::Time());
    base::TimeDelta connect_duration =
        base::Time::Now() - connect_start_time_;

    UMA_HISTOGRAM_CLIPPED_TIMES(
        FieldTrial::MakeName(
            "Net.TCP_Connection_Latency", "DnsImpact").data(),
        connect_duration,
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10),
        100);
  }

  // Now, we either succeeded at Connect()'ing, or we failed at host resolution
  // or Connect()'ing.  Either way, we'll run the callback to alert the client.

  CompletionCallback* callback = NULL;

  if (result == OK) {
    callback = pool_->OnConnectingRequestComplete(
        group_name_,
        handle_,
        false /* don't deactivate socket */,
        socket_.release());
  } else {
    callback = pool_->OnConnectingRequestComplete(
        group_name_,
        handle_,
        true /* deactivate socket */,
        NULL /* no socket to give */);
  }

  // TODO(willchan): Eventually this DCHECK will not be true, once we timeout
  // slow connecting sockets and allocate extra connecting sockets to avoid the
  // 3s timeout.
  DCHECK(callback);

  if (!synchronous)
    callback->Run(result);
  delete this;
  return result;
}

void TCPConnectingSocket::Cancel() {
  DCHECK(!canceled_);
  canceled_ = true;
}

}  // namespace net
