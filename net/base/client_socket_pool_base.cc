// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/client_socket_pool_base.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/stl_util-inl.h"
#include "net/base/client_socket_handle.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_connecting_socket.h"

using base::TimeDelta;

namespace net {

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

// InsertRequestIntoQueue inserts the request into the queue based on
// priority.  Highest priorities are closest to the front.  Older requests are
// prioritized over requests of equal priority.
void InsertRequestIntoQueue(
    const ClientSocketPoolBase::Request& r,
    ClientSocketPoolBase::RequestQueue* pending_requests) {
  ClientSocketPoolBase::RequestQueue::iterator it = pending_requests->begin();
  while (it != pending_requests->end() && r.priority <= it->priority)
    ++it;
  pending_requests->insert(it, r);
}

}  // namespace

ClientSocketPoolBase::ClientSocketPoolBase(
    int max_sockets_per_group,
    const ConnectingSocketFactory* connecting_socket_factory)
    : idle_socket_count_(0),
      max_sockets_per_group_(max_sockets_per_group),
      connecting_socket_factory_(connecting_socket_factory) {}

ClientSocketPoolBase::~ClientSocketPoolBase() {
  // Clean up any idle sockets.  Assert that we have no remaining active
  // sockets or pending requests.  They should have all been cleaned up prior
  // to the manager being destroyed.
  CleanupIdleSockets(true /* force close all idle sockets */);
  DCHECK(group_map_.empty());
}

int ClientSocketPoolBase::RequestSocket(const std::string& group_name,
                                        const Request& request) {
  DCHECK(!request.host.empty());
  DCHECK_GE(request.priority, 0);
  DCHECK(request.callback);
  DCHECK_EQ(LOAD_STATE_IDLE, request.load_state);

  Group& group = group_map_[group_name];

  // Can we make another active socket now?
  if (group.active_socket_count == max_sockets_per_group_) {
    InsertRequestIntoQueue(request, &group.pending_requests);
    return ERR_IO_PENDING;
  }

  // OK, we are going to activate one.
  group.active_socket_count++;

  while (!group.idle_sockets.empty()) {
    IdleSocket idle_socket = group.idle_sockets.back();
    group.idle_sockets.pop_back();
    DecrementIdleCount();
    if (idle_socket.socket->IsConnectedAndIdle()) {
      // We found one we can reuse!
      request.handle->set_socket(idle_socket.socket);
      request.handle->set_is_reused(true);
      return OK;
    }
    delete idle_socket.socket;
  }

  // We couldn't find a socket to reuse, so allocate and connect a new one.

  // First, we need to make sure we aren't already servicing a request for this
  // handle (which could happen if we requested, canceled, and then requested
  // with the same handle).
  if (ContainsKey(connecting_socket_map_, request.handle))
    connecting_socket_map_[request.handle]->Cancel();

  group.connecting_requests[request.handle] = request;
  group.connecting_requests[request.handle].load_state =
      LOAD_STATE_RESOLVING_HOST;

  TCPConnectingSocket* connecting_socket =
      connecting_socket_factory_->CreateConnectingSocket(group_name, request);
  connecting_socket_map_[request.handle] = connecting_socket;

  return connecting_socket->Connect();
}

void ClientSocketPoolBase::CancelRequest(const std::string& group_name,
                                        const ClientSocketHandle* handle) {
  DCHECK(ContainsKey(group_map_, group_name));

  Group& group = group_map_[group_name];

  // Search pending_requests for matching handle.
  RequestQueue::iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if (it->handle == handle) {
      group.pending_requests.erase(it);
      return;
    }
  }

  // It's invalid to cancel a non-existent request.
  DCHECK(ContainsKey(group.connecting_requests, handle));

  RequestMap::iterator map_it = group.connecting_requests.find(handle);
  if (map_it != group.connecting_requests.end()) {
    group.connecting_requests.erase(map_it);
    group.active_socket_count--;

    // Delete group if no longer needed.
    if (group.active_socket_count == 0 && group.idle_sockets.empty()) {
      DCHECK(group.pending_requests.empty());
      DCHECK(group.connecting_requests.empty());
      group_map_.erase(group_name);
    }
  }
}

void ClientSocketPoolBase::ReleaseSocket(const std::string& group_name,
                                        ClientSocket* socket) {
  // Run this asynchronously to allow the caller to finish before we let
  // another to begin doing work.  This also avoids nasty recursion issues.
  // NOTE: We cannot refer to the handle argument after this method returns.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ClientSocketPoolBase::DoReleaseSocket, group_name, socket));
}

int ClientSocketPoolBase::IdleSocketCountInGroup(
    const std::string& group_name) const {
  GroupMap::const_iterator i = group_map_.find(group_name);
  DCHECK(i != group_map_.end());

  return i->second.idle_sockets.size();
}

LoadState ClientSocketPoolBase::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  DCHECK(ContainsKey(group_map_, group_name)) << group_name;

  // Can't use operator[] since it is non-const.
  const Group& group = group_map_.find(group_name)->second;

  // Search connecting_requests for matching handle.
  RequestMap::const_iterator map_it = group.connecting_requests.find(handle);
  if (map_it != group.connecting_requests.end()) {
    const LoadState load_state = map_it->second.load_state;
    DCHECK(load_state == LOAD_STATE_RESOLVING_HOST ||
           load_state == LOAD_STATE_CONNECTING);
    return load_state;
  }

  // Search pending_requests for matching handle.
  RequestQueue::const_iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if (it->handle == handle) {
      DCHECK_EQ(LOAD_STATE_IDLE, it->load_state);
      // TODO(wtc): Add a state for being on the wait list.
      // See http://www.crbug.com/5077.
      return LOAD_STATE_IDLE;
    }
  }

  NOTREACHED();
  return LOAD_STATE_IDLE;
}

bool ClientSocketPoolBase::IdleSocket::ShouldCleanup(
    base::TimeTicks now) const {
  bool timed_out = (now - start_time) >=
      base::TimeDelta::FromSeconds(kIdleTimeout);
  return timed_out || !socket->IsConnectedAndIdle();
}

void ClientSocketPoolBase::CleanupIdleSockets(bool force) {
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
        delete j->socket;
        j = group.idle_sockets.erase(j);
        DecrementIdleCount();
      } else {
        ++j;
      }
    }

    // Delete group if no longer needed.
    if (group.active_socket_count == 0 && group.idle_sockets.empty()) {
      DCHECK(group.pending_requests.empty());
      DCHECK(group.connecting_requests.empty());
      group_map_.erase(i++);
    } else {
      ++i;
    }
  }
}

ClientSocketPoolBase::Request* ClientSocketPoolBase::GetConnectingRequest(
    const std::string& group_name, const ClientSocketHandle* handle) {
  GroupMap::iterator group_it = group_map_.find(group_name);
  if (group_it == group_map_.end())
    return NULL;

  Group& group = group_it->second;

  RequestMap* request_map = &group.connecting_requests;
  RequestMap::iterator it = request_map->find(handle);
  if (it == request_map->end())
    return NULL;

  return &it->second;
}

CompletionCallback* ClientSocketPoolBase::OnConnectingRequestComplete(
    const std::string& group_name,
    const ClientSocketHandle* handle,
    bool deactivate,
    ClientSocket* socket) {
  DCHECK((deactivate && !socket) || (!deactivate && socket));
  DCHECK(ContainsKey(group_map_, group_name));
  Group& group = group_map_[group_name];

  RequestMap* request_map = &group.connecting_requests;

  // TODO(willchan): For decoupling connecting sockets from requests, don't use
  // |handle|.  Simply pop the first request from |connecting_requests|.  Handle
  // the case where |connecting_requests| is empty by calling DoReleaseSocket().
  DCHECK(ContainsKey(*request_map, handle));
  RequestMap::iterator it = request_map->find(handle);
  Request request = it->second;
  request_map->erase(it);
  DCHECK_EQ(request.handle, handle);

  if (deactivate) {
    group.active_socket_count--;

    // Delete group if no longer needed.
    if (group.active_socket_count == 0 && group.idle_sockets.empty()) {
      DCHECK(group.pending_requests.empty());
      DCHECK(group.connecting_requests.empty());
      group_map_.erase(group_name);
    }
  } else {
    request.handle->set_socket(socket);
    request.handle->set_is_reused(false);
  }

  // TODO(willchan): Don't bother with this when decoupling connecting sockets
  // from requests.
  connecting_socket_map_.erase(handle);

  return request.callback;
}

void ClientSocketPoolBase::IncrementIdleCount() {
  if (++idle_socket_count_ == 1)
    timer_.Start(TimeDelta::FromSeconds(kCleanupInterval), this,
                 &ClientSocketPoolBase::OnCleanupTimerFired);
}

void ClientSocketPoolBase::DecrementIdleCount() {
  if (--idle_socket_count_ == 0)
    timer_.Stop();
}

void ClientSocketPoolBase::DoReleaseSocket(const std::string& group_name,
                                           ClientSocket* socket) {
  GroupMap::iterator i = group_map_.find(group_name);
  DCHECK(i != group_map_.end());

  Group& group = i->second;

  DCHECK_GT(group.active_socket_count, 0);
  group.active_socket_count--;

  const bool can_reuse = socket->IsConnectedAndIdle();
  if (can_reuse) {
    IdleSocket idle_socket;
    idle_socket.socket = socket;
    idle_socket.start_time = base::TimeTicks::Now();

    group.idle_sockets.push_back(idle_socket);
    IncrementIdleCount();
  } else {
    delete socket;
  }

  // Process one pending request.
  if (!group.pending_requests.empty()) {
    Request r = group.pending_requests.front();
    group.pending_requests.pop_front();

    int rv = RequestSocket(group_name, r);
    if (rv != ERR_IO_PENDING)
      r.callback->Run(rv);
    return;
  }

  // Delete group if no longer needed.
  if (group.active_socket_count == 0 && group.idle_sockets.empty()) {
    DCHECK(group.pending_requests.empty());
    DCHECK(group.connecting_requests.empty());
    group_map_.erase(i);
  }
}

}  // namespace net
