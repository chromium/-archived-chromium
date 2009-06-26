// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_handle.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_pool.h"

namespace net {

ClientSocketHandle::ClientSocketHandle(ClientSocketPool* pool)
    : pool_(pool),
      socket_(NULL),
      is_reused_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &ClientSocketHandle::OnIOComplete)) {}

ClientSocketHandle::~ClientSocketHandle() {
  Reset();
}

int ClientSocketHandle::Init(const std::string& group_name,
                             const HostResolver::RequestInfo& resolve_info,
                             int priority,
                             CompletionCallback* callback) {
  ResetInternal(true);
  group_name_ = group_name;
  int rv = pool_->RequestSocket(
      group_name, resolve_info, priority, this, &callback_);
  if (rv == ERR_IO_PENDING) {
    user_callback_ = callback;
  } else {
    HandleInitCompletion(rv);
  }
  return rv;
}

void ClientSocketHandle::Reset() {
  ResetInternal(true);
}

void ClientSocketHandle::ResetInternal(bool cancel) {
  if (group_name_.empty())  // Was Init called?
    return;
  if (socket_.get()) {
    // If we've still got a socket, release it back to the ClientSocketPool so
    // it can be deleted or reused.
    pool_->ReleaseSocket(group_name_, release_socket());
  } else if (cancel) {
    // If we did not get initialized yet, so we've got a socket request pending.
    // Cancel it.
    pool_->CancelRequest(group_name_, this);
  }
  group_name_.clear();
  is_reused_ = false;
  user_callback_ = NULL;
}

LoadState ClientSocketHandle::GetLoadState() const {
  CHECK(!is_initialized());
  CHECK(!group_name_.empty());
  return pool_->GetLoadState(group_name_, this);
}

void ClientSocketHandle::OnIOComplete(int result) {
  CompletionCallback* callback = user_callback_;
  user_callback_ = NULL;
  HandleInitCompletion(result);
  callback->Run(result);
}

void ClientSocketHandle::HandleInitCompletion(int result) {
  CHECK(ERR_IO_PENDING != result);
  if (result != OK)
    ResetInternal(false);  // The request failed, so there's nothing to cancel.
}

}  // namespace net
