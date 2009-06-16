// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_CLIENT_SOCKET_POOL_H_
#define NET_BASE_TCP_CLIENT_SOCKET_POOL_H_

#include <deque>
#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "net/base/address_list.h"
#include "net/base/client_socket_pool.h"
#include "net/base/host_resolver.h"

namespace net {

class ClientSocketFactory;
class TCPClientSocketPool;

// ConnectingSocket handles the host resolution necessary for socket creation
// and the socket Connect().
class ConnectingSocket {
 public:
  ConnectingSocket(const std::string& group_name,
                   const HostResolver::RequestInfo& resolve_info,
                   const ClientSocketHandle* handle,
                   ClientSocketFactory* client_socket_factory,
                   TCPClientSocketPool* pool);
  ~ConnectingSocket();

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int Connect();

  // Called by the TCPClientSocketPool to cancel this ConnectingSocket.  Only
  // necessary if a ClientSocketHandle is reused.
  void Cancel();

 private:
  // Handles asynchronous completion of IO.  |result| represents the result of
  // the IO operation.
  void OnIOComplete(int result);

  // Handles both asynchronous and synchronous completion of IO.  |result|
  // represents the result of the IO operation.  |synchronous| indicates
  // whether or not the previous IO operation completed synchronously or
  // asynchronously.  OnIOCompleteInternal returns the result of the next IO
  // operation that executes, or just the value of |result|.
  int OnIOCompleteInternal(int result, bool synchronous);

  const std::string group_name_;
  const HostResolver::RequestInfo resolve_info_;
  const ClientSocketHandle* const handle_;
  ClientSocketFactory* const client_socket_factory_;
  CompletionCallbackImpl<ConnectingSocket> callback_;
  scoped_ptr<ClientSocket> socket_;
  scoped_refptr<TCPClientSocketPool> pool_;
  SingleRequestHostResolver resolver_;
  AddressList addresses_;

  // The time the Connect() method was called (if it got called).
  base::TimeTicks connect_start_time_;

  DISALLOW_COPY_AND_ASSIGN(ConnectingSocket);
};

// A TCPClientSocketPool is used to restrict the number of TCP sockets open at
// a time.  It also maintains a list of idle persistent sockets.
//
class TCPClientSocketPool : public ClientSocketPool {
 public:
  // A Request is allocated per call to RequestSocket that results in
  // ERR_IO_PENDING.
  struct Request {
    // HostResolver::RequestInfo has no default constructor, so fudge something.
    Request() : resolve_info(std::string(), 0) {}

    Request(ClientSocketHandle* handle,
            CompletionCallback* callback,
            int priority,
            const HostResolver::RequestInfo& resolve_info,
            LoadState load_state)
        : handle(handle), callback(callback), priority(priority),
          resolve_info(resolve_info), load_state(load_state) {
    }

    ClientSocketHandle* handle;
    CompletionCallback* callback;
    int priority;
    HostResolver::RequestInfo resolve_info;
    LoadState load_state;
  };

  TCPClientSocketPool(int max_sockets_per_group,
                      HostResolver* host_resolver,
                      ClientSocketFactory* client_socket_factory);

  // ClientSocketPool methods:

  virtual int RequestSocket(const std::string& group_name,
                            const HostResolver::RequestInfo& resolve_info,
                            int priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback);

  virtual void CancelRequest(const std::string& group_name,
                             const ClientSocketHandle* handle);

  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket);

  virtual void CloseIdleSockets();

  virtual HostResolver* GetHostResolver() const {
    return host_resolver_;
  }

  virtual int idle_socket_count() const {
    return idle_socket_count_;
  }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const;

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const;

  // Used by ConnectingSocket until we remove the coupling between a specific
  // ConnectingSocket and a ClientSocketHandle:
 
  // Returns NULL if not found.  Otherwise it returns the Request*
  // corresponding to the ConnectingSocket (keyed by |group_name| and |handle|.
  // Note that this pointer may be invalidated after any call that might mutate
  // the RequestMap or GroupMap, so the user should not hold onto the pointer
  // for long.
  Request* GetConnectingRequest(const std::string& group_name,
                                const ClientSocketHandle* handle);
 
  // Handles the completed Request corresponding to the ConnectingSocket (keyed
  // by |group_name| and |handle|.  |deactivate| indicates whether or not to
  // deactivate the socket, making the socket slot available for a new socket
  // connection.  If |deactivate| is false, then set |socket| into |handle|.
  // Returns the callback to run.
  CompletionCallback* OnConnectingRequestComplete(
      const std::string& group_name,
      const ClientSocketHandle* handle,
      bool deactivate,
      ClientSocket* socket);

 private:
  // Entry for a persistent socket which became idle at time |start_time|.
  struct IdleSocket {
    ClientSocket* socket;
    base::TimeTicks start_time;

    // An idle socket should be removed if it can't be reused, or has been idle
    // for too long. |now| is the current time value (TimeTicks::Now()).
    //
    // An idle socket can't be reused if it is disconnected or has received
    // data unexpectedly (hence no longer idle).  The unread data would be
    // mistaken for the beginning of the next response if we were to reuse the
    // socket for a new request.
    bool ShouldCleanup(base::TimeTicks now) const;
  };

  typedef std::deque<Request> RequestQueue;
  typedef std::map<const ClientSocketHandle*, Request> RequestMap;

  // A Group is allocated per group_name when there are idle sockets or pending
  // requests.  Otherwise, the Group object is removed from the map.
  struct Group {
    Group() : active_socket_count(0) {}
    std::deque<IdleSocket> idle_sockets;
    RequestQueue pending_requests;
    RequestMap connecting_requests;
    int active_socket_count;
  };

  typedef std::map<std::string, Group> GroupMap;

  typedef std::map<const ClientSocketHandle*, ConnectingSocket*>
      ConnectingSocketMap;

  virtual ~TCPClientSocketPool();

  static void InsertRequestIntoQueue(const Request& r,
                                     RequestQueue* pending_requests);

  // Closes all idle sockets if |force| is true.  Else, only closes idle
  // sockets that timed out or can't be reused.
  void CleanupIdleSockets(bool force);

  // Called when the number of idle sockets changes.
  void IncrementIdleCount();
  void DecrementIdleCount();

  // Called via PostTask by ReleaseSocket.
  void DoReleaseSocket(const std::string& group_name, ClientSocket* socket);

  // Called when timer_ fires.  This method scans the idle sockets removing
  // sockets that timed out or can't be reused.
  void OnCleanupTimerFired() {
    CleanupIdleSockets(false);
  }

  // Removes the ConnectingSocket corresponding to |handle| from the
  // |connecting_socket_map_|.
  void RemoveConnectingSocket(const ClientSocketHandle* handle);

  ClientSocketFactory* const client_socket_factory_;

  GroupMap group_map_;

  ConnectingSocketMap connecting_socket_map_;

  // Timer used to periodically prune idle sockets that timed out or can't be
  // reused.
  base::RepeatingTimer<TCPClientSocketPool> timer_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // The maximum number of sockets kept per group.
  const int max_sockets_per_group_;

  // The host resolver that will be used to do DNS lookups for connecting
  // sockets.
  HostResolver* host_resolver_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketPool);
};

}  // namespace net

#endif  // NET_BASE_TCP_CLIENT_SOCKET_POOL_H_
