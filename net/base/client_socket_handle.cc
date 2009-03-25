// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/client_socket_handle.h"

#include "net/base/client_socket.h"
#include "net/base/client_socket_pool.h"

namespace net {

ClientSocketHandle::ClientSocketHandle(ClientSocketPool* pool)
    : pool_(pool), socket_(NULL) {
}

ClientSocketHandle::~ClientSocketHandle() {
  Reset();
}

int ClientSocketHandle::Init(const std::string& group_name,
                             int priority,
                             CompletionCallback* callback) {
  Reset();
  group_name_ = group_name;
  return pool_->RequestSocket(this, priority, callback);
}

void ClientSocketHandle::Reset() {
  if (group_name_.empty())  // Was Init called?
    return;
  if (socket_) {
    pool_->ReleaseSocket(this);
    socket_ = NULL;
  } else {
    pool_->CancelRequest(this);
  }
  group_name_.clear();
}

}  // namespace net
