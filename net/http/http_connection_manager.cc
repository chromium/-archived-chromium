// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/http/http_connection_manager.h"

#include "base/message_loop.h"
#include "net/base/client_socket.h"
#include "net/base/net_errors.h"

namespace {

// The timeout value, in seconds, used to clean up disconnected idle sockets.
const int kCleanupInterval = 5;

}  // namespace

namespace net {

HttpConnectionManager::HttpConnectionManager()
    : timer_(TimeDelta::FromSeconds(kCleanupInterval)),
      idle_socket_count_(0) {
  timer_.set_task(this);
}

HttpConnectionManager::~HttpConnectionManager() {
  timer_.set_task(NULL);

  // Clean up any idle sockets.  Assert that we have no remaining active sockets
  // or pending requests.  They should have all been cleaned up prior to the
  // manager being destroyed.

  CloseIdleSockets();
  DCHECK(group_map_.empty());
}

int HttpConnectionManager::RequestSocket(const std::string& group_name,
                                         SocketHandle** handle,
                                         CompletionCallback* callback) {
  Group& group = group_map_[group_name];

  // Can we make another active socket now?
  if (group.active_socket_count == kMaxSocketsPerGroup) {
    Request r;
    r.result = handle;
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
    SocketHandle* h = group.idle_sockets.back();
    group.idle_sockets.pop_back();
    DecrementIdleCount();
    if (h->get()->IsConnected()) {
      // We found one we can reuse!
      *handle = h;
      return OK;
    }
    delete h;
  }

  *handle = new SocketHandle();
  return OK;
}

void HttpConnectionManager::CancelRequest(const std::string& group_name,
                                          SocketHandle** handle) {
  Group& group = group_map_[group_name];

  // In order for us to be canceling a pending request, we must have active
  // sockets equaling the limit.  NOTE: The correctness of the code doesn't
  // require this assertion.
  DCHECK(group.active_socket_count == kMaxSocketsPerGroup);

  // Search pending_requests for matching handle.
  std::deque<Request>::iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if (it->result == handle) {
      group.pending_requests.erase(it);
      break;
    }
  }
}

void HttpConnectionManager::ReleaseSocket(const std::string& group_name,
                                          SocketHandle* handle) {
  // Run this asynchronously to allow the caller to finish before we let
  // another to begin doing work.  This also avoids nasty recursion issues.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &HttpConnectionManager::DoReleaseSocket, group_name, handle));
}

void HttpConnectionManager::CloseIdleSockets() {
  MaybeCloseIdleSockets(false);
}

void HttpConnectionManager::MaybeCloseIdleSockets(
    bool only_if_disconnected) {
  if (idle_socket_count_ == 0)
    return;

  GroupMap::iterator i = group_map_.begin();
  while (i != group_map_.end()) {
    Group& group = i->second;

    std::deque<SocketHandle*>::iterator j = group.idle_sockets.begin();
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

void HttpConnectionManager::IncrementIdleCount() {
  if (++idle_socket_count_ == 1)
    timer_.Start();
}

void HttpConnectionManager::DecrementIdleCount() {
  if (--idle_socket_count_ == 0)
    timer_.Stop();
}

void HttpConnectionManager::DoReleaseSocket(const std::string& group_name,
                                            SocketHandle* handle) {
  GroupMap::iterator i = group_map_.find(group_name);
  DCHECK(i != group_map_.end());

  Group& group = i->second;

  DCHECK(group.active_socket_count > 0);
  group.active_socket_count--;

  bool can_reuse = handle->get() && handle->get()->IsConnected();
  if (can_reuse) {
    group.idle_sockets.push_back(handle);
    IncrementIdleCount();
  } else {
    delete handle;
  }

  // Process one pending request.
  if (!group.pending_requests.empty()) {
    Request r = group.pending_requests.front();
    group.pending_requests.pop_front();
    int rv = RequestSocket(i->first, r.result, NULL);
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

void HttpConnectionManager::Run() {
  MaybeCloseIdleSockets(true);
}

}  // namespace net
