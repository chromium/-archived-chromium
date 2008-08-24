// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/client_socket_pool.h"

#include "base/message_loop.h"
#include "net/base/client_socket.h"
#include "net/base/client_socket_handle.h"
#include "net/base/net_errors.h"

namespace {

// The timeout value, in seconds, used to clean up disconnected idle sockets.
const int kCleanupInterval = 5;

}  // namespace

namespace net {

ClientSocketPool::ClientSocketPool(int max_sockets_per_group)
    : timer_(TimeDelta::FromSeconds(kCleanupInterval)),
      idle_socket_count_(0),
      max_sockets_per_group_(max_sockets_per_group) {
  timer_.set_task(this);
}

ClientSocketPool::~ClientSocketPool() {
  timer_.set_task(NULL);

  // Clean up any idle sockets.  Assert that we have no remaining active sockets
  // or pending requests.  They should have all been cleaned up prior to the
  // manager being destroyed.

  CloseIdleSockets();
  DCHECK(group_map_.empty());
}

int ClientSocketPool::RequestSocket(ClientSocketHandle* handle,
                                    CompletionCallback* callback) {
  Group& group = group_map_[handle->group_name_];

  // Can we make another active socket now?
  if (group.active_socket_count == max_sockets_per_group_) {
    Request r;
    r.handle = handle;
    DCHECK(callback);
    r.callback = callback;
    group.pending_requests.push_back(r);
    return ERR_IO_PENDING;
  }

  // OK, we are going to activate one.
  group.active_socket_count++;

  // Use idle sockets in LIFO order because they're more likely to be
  // still connected.
  while (!group.idle_sockets.empty()) {
    ClientSocketPtr* ptr = group.idle_sockets.back();
    group.idle_sockets.pop_back();
    DecrementIdleCount();
    if ((*ptr)->IsConnected()) {
      // We found one we can reuse!
      handle->socket_ = ptr;
      return OK;
    }
    delete ptr;
  }

  handle->socket_ = new ClientSocketPtr();
  return OK;
}

void ClientSocketPool::CancelRequest(ClientSocketHandle* handle) {
  Group& group = group_map_[handle->group_name_];

  // In order for us to be canceling a pending request, we must have active
  // sockets equaling the limit.  NOTE: The correctness of the code doesn't
  // require this assertion.
  DCHECK(group.active_socket_count == max_sockets_per_group_);

  // Search pending_requests for matching handle.
  std::deque<Request>::iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if (it->handle == handle) {
      group.pending_requests.erase(it);
      break;
    }
  }
}

void ClientSocketPool::ReleaseSocket(ClientSocketHandle* handle) {
  // Run this asynchronously to allow the caller to finish before we let
  // another to begin doing work.  This also avoids nasty recursion issues.
  // NOTE: We cannot refer to the handle argument after this method returns.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ClientSocketPool::DoReleaseSocket, handle->group_name_,
      handle->socket_));
}

void ClientSocketPool::CloseIdleSockets() {
  MaybeCloseIdleSockets(false);
}

void ClientSocketPool::MaybeCloseIdleSockets(
    bool only_if_disconnected) {
  if (idle_socket_count_ == 0)
    return;

  GroupMap::iterator i = group_map_.begin();
  while (i != group_map_.end()) {
    Group& group = i->second;

    std::deque<ClientSocketPtr*>::iterator j = group.idle_sockets.begin();
    while (j != group.idle_sockets.end()) {
      if (!only_if_disconnected || !(*j)->get()->IsConnected()) {
        delete *j;
        j = group.idle_sockets.erase(j);
        DecrementIdleCount();
      } else {
        ++j;
      }
    }

    // Delete group if no longer needed.
    if (group.active_socket_count == 0 && group.idle_sockets.empty()) {
      DCHECK(group.pending_requests.empty());
      group_map_.erase(i++);
    } else {
      ++i;
    }
  }
}

void ClientSocketPool::IncrementIdleCount() {
  if (++idle_socket_count_ == 1)
    timer_.Start();
}

void ClientSocketPool::DecrementIdleCount() {
  if (--idle_socket_count_ == 0)
    timer_.Stop();
}

void ClientSocketPool::DoReleaseSocket(const std::string& group_name,
                                       ClientSocketPtr* ptr) {
  GroupMap::iterator i = group_map_.find(group_name);
  DCHECK(i != group_map_.end());

  Group& group = i->second;

  DCHECK(group.active_socket_count > 0);
  group.active_socket_count--;

  bool can_reuse = ptr->get() && (*ptr)->IsConnected();
  if (can_reuse) {
    group.idle_sockets.push_back(ptr);
    IncrementIdleCount();
  } else {
    delete ptr;
  }

  // Process one pending request.
  if (!group.pending_requests.empty()) {
    Request r = group.pending_requests.front();
    group.pending_requests.pop_front();
    int rv = RequestSocket(r.handle, NULL);
    DCHECK(rv == OK);
    r.callback->Run(rv);
    return;
  }

  // Delete group if no longer needed.
  if (group.active_socket_count == 0 && group.idle_sockets.empty()) {
    DCHECK(group.pending_requests.empty());
    group_map_.erase(i);
  }
}

void ClientSocketPool::Run() {
  MaybeCloseIdleSockets(true);
}

}  // namespace net

