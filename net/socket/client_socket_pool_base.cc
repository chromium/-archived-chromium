// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_handle.h"

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

ConnectJob::ConnectJob(const std::string& group_name,
                       const ClientSocketHandle* key_handle,
                       Delegate* delegate)
    : group_name_(group_name),
      key_handle_(key_handle),
      delegate_(delegate),
      load_state_(LOAD_STATE_IDLE) {
  DCHECK(!group_name.empty());
  DCHECK(key_handle);
  DCHECK(delegate);
}

ConnectJob::~ConnectJob() {}

ClientSocketPoolBase::ClientSocketPoolBase(
    int max_sockets_per_group,
    ConnectJobFactory* connect_job_factory)
    : idle_socket_count_(0),
      max_sockets_per_group_(max_sockets_per_group),
      connect_job_factory_(connect_job_factory) {}

ClientSocketPoolBase::~ClientSocketPoolBase() {
  // Clean up any idle sockets.  Assert that we have no remaining active
  // sockets or pending requests.  They should have all been cleaned up prior
  // to the manager being destroyed.
  CloseIdleSockets();
  DCHECK(group_map_.empty());
  DCHECK(connect_job_map_.empty());
}

// InsertRequestIntoQueue inserts the request into the queue based on
// priority.  Highest priorities are closest to the front.  Older requests are
// prioritized over requests of equal priority.
//
// static
void ClientSocketPoolBase::InsertRequestIntoQueue(
    const Request& r, RequestQueue* pending_requests) {
  RequestQueue::iterator it = pending_requests->begin();
  while (it != pending_requests->end() && r.priority <= it->priority)
    ++it;
  pending_requests->insert(it, r);
}

int ClientSocketPoolBase::RequestSocket(
    const std::string& group_name,
    const HostResolver::RequestInfo& resolve_info,
    int priority,
    ClientSocketHandle* handle,
    CompletionCallback* callback) {
  DCHECK(!resolve_info.hostname().empty());
  DCHECK_GE(priority, 0);
  DCHECK(callback);
  Group& group = group_map_[group_name];

  // Can we make another active socket now?
  if (!group.HasAvailableSocketSlot(max_sockets_per_group_)) {
    CHECK(callback);
    Request r(handle, callback, priority, resolve_info);
    InsertRequestIntoQueue(r, &group.pending_requests);
    return ERR_IO_PENDING;
  }

  while (!group.idle_sockets.empty()) {
    IdleSocket idle_socket = group.idle_sockets.back();
    group.idle_sockets.pop_back();
    DecrementIdleCount();
    if (idle_socket.socket->IsConnectedAndIdle()) {
      // We found one we can reuse!
      HandOutSocket(idle_socket.socket, true /* reuse */, handle, &group);
      return OK;
    }
    delete idle_socket.socket;
  }

  // We couldn't find a socket to reuse, so allocate and connect a new one.

  CHECK(callback);
  Request r(handle, callback, priority, resolve_info);
  scoped_ptr<ConnectJob> connect_job(
      connect_job_factory_->NewConnectJob(group_name, r, this));

  int rv = connect_job->Connect();
  if (rv == OK) {
    HandOutSocket(connect_job->ReleaseSocket(), false /* not reused */,
                  handle, &group);
  } else if (rv == ERR_IO_PENDING) {
    group.connecting_requests[handle] = r;
    CHECK(!ContainsKey(connect_job_map_, handle));
    connect_job_map_[handle] = connect_job.release();
  } else {
    if (group.IsEmpty())
      group_map_.erase(group_name);
  }

  return rv;
}

void ClientSocketPoolBase::CancelRequest(const std::string& group_name,
                                         const ClientSocketHandle* handle) {
  CHECK(ContainsKey(group_map_, group_name));

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
  CHECK(ContainsKey(group.connecting_requests, handle));

  RequestMap::iterator map_it = group.connecting_requests.find(handle);
  if (map_it != group.connecting_requests.end()) {
    RemoveConnectJob(handle);
    group.connecting_requests.erase(map_it);
    OnAvailableSocketSlot(group_name, &group);
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

void ClientSocketPoolBase::CloseIdleSockets() {
  CleanupIdleSockets(true);
}

int ClientSocketPoolBase::IdleSocketCountInGroup(
    const std::string& group_name) const {
  GroupMap::const_iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  return i->second.idle_sockets.size();
}

LoadState ClientSocketPoolBase::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  if (!ContainsKey(group_map_, group_name)) {
    NOTREACHED() << "ClientSocketPool does not contain group: " << group_name
                 << " for handle: " << handle;
    return LOAD_STATE_IDLE;
  }

  // Can't use operator[] since it is non-const.
  const Group& group = group_map_.find(group_name)->second;

  // Search connecting_requests for matching handle.
  RequestMap::const_iterator map_it = group.connecting_requests.find(handle);
  if (map_it != group.connecting_requests.end()) {
    ConnectJobMap::const_iterator job_it = connect_job_map_.find(handle);
    if (job_it == connect_job_map_.end()) {
      NOTREACHED();
      return LOAD_STATE_IDLE;
    }
    return job_it->second->load_state();
  }

  // Search pending_requests for matching handle.
  RequestQueue::const_iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if (it->handle == handle) {
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
    if (group.IsEmpty()) {
      CHECK(group.pending_requests.empty());
      group_map_.erase(i++);
    } else {
      ++i;
    }
  }
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
  CHECK(i != group_map_.end());

  Group& group = i->second;

  CHECK(group.active_socket_count > 0);
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

  OnAvailableSocketSlot(group_name, &group);
}

void ClientSocketPoolBase::OnConnectJobComplete(int result, ConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);
  const std::string group_name = job->group_name();
  GroupMap::iterator group_it = group_map_.find(group_name);
  CHECK(group_it != group_map_.end());
  Group& group = group_it->second;

  RequestMap* request_map = &group.connecting_requests;

  RequestMap::iterator it = request_map->find(job->key_handle());
  CHECK(it != request_map->end());
  ClientSocketHandle* const handle = it->second.handle;
  CompletionCallback* const callback = it->second.callback;
  request_map->erase(it);
  DCHECK_EQ(handle, job->key_handle());

  scoped_ptr<ClientSocket> socket(job->ReleaseSocket());
  RemoveConnectJob(job->key_handle());

  if (result != OK) {
    callback->Run(result);  // |group| is not necessarily valid after this.
    // |group| may be invalid after the callback, we need to search
    // |group_map_| again.
    MaybeOnAvailableSocketSlot(group_name);
  } else {
    HandOutSocket(socket.release(), false /* not reused */, handle, &group);
    callback->Run(result);
  }
}

void ClientSocketPoolBase::RemoveConnectJob(
    const ClientSocketHandle* handle) {
  ConnectJobMap::iterator it = connect_job_map_.find(handle);
  CHECK(it != connect_job_map_.end());
  delete it->second;
  connect_job_map_.erase(it);
}

void ClientSocketPoolBase::MaybeOnAvailableSocketSlot(
    const std::string& group_name) {
  GroupMap::iterator it = group_map_.find(group_name);
  if (it != group_map_.end()) {
    Group& group = it->second;
    if (group.HasAvailableSocketSlot(max_sockets_per_group_))
      OnAvailableSocketSlot(group_name, &group);
  }
}

void ClientSocketPoolBase::OnAvailableSocketSlot(const std::string& group_name,
                                                 Group* group) {
  if (!group->pending_requests.empty()) {
    ProcessPendingRequest(group_name, group);
    // |group| may no longer be valid after this point.  Be careful not to
    // access it again.
  } else if (group->IsEmpty()) {
    // Delete |group| if no longer needed.  |group| will no longer be valid.
    group_map_.erase(group_name);
  }
}

void ClientSocketPoolBase::ProcessPendingRequest(const std::string& group_name,
                                                 Group* group) {
  Request r = group->pending_requests.front();
  group->pending_requests.pop_front();

  int rv = RequestSocket(
      group_name, r.resolve_info, r.priority, r.handle, r.callback);

  if (rv != ERR_IO_PENDING) {
    r.callback->Run(rv);
    if (rv != OK) {
      // |group| may be invalid after the callback, we need to search
      // |group_map_| again.
      MaybeOnAvailableSocketSlot(group_name);
    }
  }
}

void ClientSocketPoolBase::HandOutSocket(
    ClientSocket* socket,
    bool reused,
    ClientSocketHandle* handle,
    Group* group) {
  DCHECK(socket);
  handle->set_socket(socket);
  handle->set_is_reused(reused);
  group->active_socket_count++;
}

}  // namespace net
