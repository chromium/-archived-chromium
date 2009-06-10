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

TCPClientSocketPool::ConnectingSocket::ConnectingSocket(
    const std::string& group_name,
    const ClientSocketHandle* handle,
    ClientSocketFactory* client_socket_factory,
    TCPClientSocketPool* pool)
    : group_name_(group_name),
      handle_(handle),
      client_socket_factory_(client_socket_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this,
                    &TCPClientSocketPool::ConnectingSocket::OnIOComplete)),
      pool_(pool),
      canceled_(false) {
  DCHECK(!ContainsKey(pool_->connecting_socket_map_, handle));
  pool_->connecting_socket_map_[handle] = this;
}

TCPClientSocketPool::ConnectingSocket::~ConnectingSocket() {
  if (!canceled_)
    pool_->connecting_socket_map_.erase(handle_);
}

int TCPClientSocketPool::ConnectingSocket::Connect(
    const std::string& host,
    int port) {
  DCHECK(!canceled_);
  DidStartDnsResolution(host, this);
  int rv = resolver_.Resolve(host, port, &addresses_, &callback_);
  if (rv != ERR_IO_PENDING)
    rv = OnIOCompleteInternal(rv, true /* synchronous */);
  return rv;
}

void TCPClientSocketPool::ConnectingSocket::OnIOComplete(int result) {
  OnIOCompleteInternal(result, false /* asynchronous */);
}

int TCPClientSocketPool::ConnectingSocket::OnIOCompleteInternal(
    int result, bool synchronous) {
  DCHECK_NE(result, ERR_IO_PENDING);

  if (canceled_) {
    // We got canceled, so bail out.
    delete this;
    return result;
  }

  GroupMap::iterator group_it = pool_->group_map_.find(group_name_);
  if (group_it == pool_->group_map_.end()) {
    // The request corresponding to this ConnectingSocket has been canceled.
    // Stop bothering with it.
    delete this;
    return result;
  }

  Group& group = group_it->second;

  RequestMap* request_map = &group.connecting_requests;
  RequestMap::iterator it = request_map->find(handle_);
  if (it == request_map->end()) {
    // The request corresponding to this ConnectingSocket has been canceled.
    // Stop bothering with it.
    delete this;
    return result;
  }

  if (result == OK && it->second.load_state == LOAD_STATE_RESOLVING_HOST) {
    it->second.load_state = LOAD_STATE_CONNECTING;
    socket_.reset(client_socket_factory_->CreateTCPClientSocket(addresses_));
    connect_start_time_ = base::Time::Now();
    result = socket_->Connect(&callback_);
    if (result == ERR_IO_PENDING)
      return result;
  }

  if (result == OK) {
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

  Request request = it->second;
  request_map->erase(it);

  if (result == OK) {
    request.handle->set_socket(socket_.release());
    request.handle->set_is_reused(false);
  } else {
    group.active_socket_count--;

    // Delete group if no longer needed.
    if (group.active_socket_count == 0 && group.idle_sockets.empty()) {
      DCHECK(group.pending_requests.empty());
      DCHECK(group.connecting_requests.empty());
      pool_->group_map_.erase(group_it);
    }
  }

  if (!synchronous)
    request.callback->Run(result);
  delete this;
  return result;
}

void TCPClientSocketPool::ConnectingSocket::Cancel() {
  DCHECK(!canceled_);
  DCHECK(ContainsKey(pool_->connecting_socket_map_, handle_));
  pool_->connecting_socket_map_.erase(handle_);
  canceled_ = true;
}

TCPClientSocketPool::TCPClientSocketPool(
    int max_sockets_per_group,
    ClientSocketFactory* client_socket_factory)
    : client_socket_factory_(client_socket_factory),
      idle_socket_count_(0),
      max_sockets_per_group_(max_sockets_per_group) {
}

TCPClientSocketPool::~TCPClientSocketPool() {
  // Clean up any idle sockets.  Assert that we have no remaining active
  // sockets or pending requests.  They should have all been cleaned up prior
  // to the manager being destroyed.
  CloseIdleSockets();
  DCHECK(group_map_.empty());
}

// InsertRequestIntoQueue inserts the request into the queue based on
// priority.  Highest priorities are closest to the front.  Older requests are
// prioritized over requests of equal priority.
//
// static
void TCPClientSocketPool::InsertRequestIntoQueue(
    const Request& r, RequestQueue* pending_requests) {
  RequestQueue::iterator it = pending_requests->begin();
  while (it != pending_requests->end() && r.priority <= it->priority)
    ++it;
  pending_requests->insert(it, r);
}

int TCPClientSocketPool::RequestSocket(const std::string& group_name,
                                       const std::string& host,
                                       int port,
                                       int priority,
                                       ClientSocketHandle* handle,
                                       CompletionCallback* callback) {
  DCHECK(!host.empty());
  DCHECK_GE(priority, 0);
  Group& group = group_map_[group_name];

  // Can we make another active socket now?
  if (group.active_socket_count == max_sockets_per_group_) {
    Request r;
    r.handle = handle;
    DCHECK(callback);
    r.callback = callback;
    r.priority = priority;
    r.host = host;
    r.port = port;
    r.load_state = LOAD_STATE_IDLE;
    InsertRequestIntoQueue(r, &group.pending_requests);
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
      handle->set_socket(idle_socket.socket);
      handle->set_is_reused(true);
      return OK;
    }
    delete idle_socket.socket;
  }

  // We couldn't find a socket to reuse, so allocate and connect a new one.

  // First, we need to make sure we aren't already servicing a request for this
  // handle (which could happen if we requested, canceled, and then requested
  // with the same handle).
  if (ContainsKey(connecting_socket_map_, handle))
    connecting_socket_map_[handle]->Cancel();

  Request r;
  r.handle = handle;
  DCHECK(callback);
  r.callback = callback;
  r.priority = priority;
  r.host = host;
  r.port = port;
  r.load_state = LOAD_STATE_RESOLVING_HOST;
  group_map_[group_name].connecting_requests[handle] = r;

  // connecting_socket will delete itself.
  ConnectingSocket* connecting_socket =
      new ConnectingSocket(group_name, handle, client_socket_factory_, this);
  int rv = connecting_socket->Connect(host, port);
  return rv;
}

void TCPClientSocketPool::CancelRequest(const std::string& group_name,
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

void TCPClientSocketPool::ReleaseSocket(const std::string& group_name,
                                        ClientSocket* socket) {
  // Run this asynchronously to allow the caller to finish before we let
  // another to begin doing work.  This also avoids nasty recursion issues.
  // NOTE: We cannot refer to the handle argument after this method returns.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &TCPClientSocketPool::DoReleaseSocket, group_name, socket));
}

void TCPClientSocketPool::CloseIdleSockets() {
  CleanupIdleSockets(true);
}

int TCPClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  GroupMap::const_iterator i = group_map_.find(group_name);
  DCHECK(i != group_map_.end());

  return i->second.idle_sockets.size();
}

LoadState TCPClientSocketPool::GetLoadState(
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

bool TCPClientSocketPool::IdleSocket::ShouldCleanup(base::TimeTicks now) const {
  bool timed_out = (now - start_time) >=
      base::TimeDelta::FromSeconds(kIdleTimeout);
  return timed_out || !socket->IsConnectedAndIdle();
}

void TCPClientSocketPool::CleanupIdleSockets(bool force) {
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

void TCPClientSocketPool::IncrementIdleCount() {
  if (++idle_socket_count_ == 1)
    timer_.Start(TimeDelta::FromSeconds(kCleanupInterval), this,
                 &TCPClientSocketPool::OnCleanupTimerFired);
}

void TCPClientSocketPool::DecrementIdleCount() {
  if (--idle_socket_count_ == 0)
    timer_.Stop();
}

void TCPClientSocketPool::DoReleaseSocket(const std::string& group_name,
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

    int rv = RequestSocket(
        group_name, r.host, r.port, r.priority, r.handle, r.callback);
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
