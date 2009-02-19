// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/client_socket_pool.h"

#include "base/message_loop.h"
#include "net/base/client_socket.h"
#include "net/base/client_socket_handle.h"
#include "net/base/net_errors.h"

using base::TimeDelta;

namespace {

// The timeout value, in seconds, used to clean up idle sockets that can't be
// reused.
const int kCleanupInterval = 10;

// The maximum duration, in seconds, to keep idle persistent sockets alive.
const int kIdleTimeout = 300;  // 5 minutes.

}  // namespace

namespace net {

ClientSocketPool::ClientSocketPool(int max_sockets_per_group)
    : idle_socket_count_(0),
      max_sockets_per_group_(max_sockets_per_group) {
}

ClientSocketPool::~ClientSocketPool() {
  // Clean up any idle sockets.  Assert that we have no remaining active
  // sockets or pending requests.  They should have all been cleaned up prior
  // to the manager being destroyed.
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
  // still reusable.
  while (!group.idle_sockets.empty()) {
    IdleSocket idle_socket = group.idle_sockets.back();
    group.idle_sockets.pop_back();
    DecrementIdleCount();
    if ((*idle_socket.ptr)->IsConnectedAndIdle()) {
      // We found one we can reuse!
      handle->socket_ = idle_socket.ptr;
      return OK;
    }
    delete idle_socket.ptr;
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
  CleanupIdleSockets(true);
}

bool ClientSocketPool::IdleSocket::ShouldCleanup(base::TimeTicks now) const {
  bool timed_out = (now - start_time) >=
      base::TimeDelta::FromSeconds(kIdleTimeout);
  return timed_out || !(*ptr)->IsConnectedAndIdle();
}

void ClientSocketPool::CleanupIdleSockets(bool force) {
  if (idle_socket_count_ == 0)
    return;

  // Current time value. Retrieving it once at the function start rather than
  // inside the inner loop, since it shouldn't change by any meaningful amount.
  base::TimeTicks now = base::TimeTicks::Now();

  GroupMap::iterator i = group_map_.begin();
  while (i != group_map_.end()) {
    Group& group = i->second;

    std::deque<IdleSocket>::iterator j = group.idle_sockets.begin();
    while (j != group.idle_sockets.end()) {
      if (force || j->ShouldCleanup(now)) {
        delete j->ptr;
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
    timer_.Start(TimeDelta::FromSeconds(kCleanupInterval), this,
                 &ClientSocketPool::OnCleanupTimerFired);
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

  bool can_reuse = ptr->get() && (*ptr)->IsConnectedAndIdle();
  if (can_reuse) {
    IdleSocket idle_socket;
    idle_socket.ptr = ptr;
    idle_socket.start_time = base::TimeTicks::Now();

    group.idle_sockets.push_back(idle_socket);
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

}  // namespace net

