// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CLIENT_SOCKET_POOL_H_
#define NET_BASE_CLIENT_SOCKET_POOL_H_

#include <deque>
#include <map>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/load_states.h"

namespace net {

class ClientSocket;
class ClientSocketFactory;
class ClientSocketHandle;

// A ClientSocketPool is used to restrict the number of sockets open at a time.
// It also maintains a list of idle persistent sockets.
//
class ClientSocketPool : public base::RefCounted<ClientSocketPool> {
 public:
  ClientSocketPool(int max_sockets_per_group,
                   ClientSocketFactory* client_socket_factory);

  // Requests a connected socket for a group_name.
  //
  // There are four possible results from calling this function:
  // 1) RequestSocket returns OK and initializes |handle| with a reused socket.
  // 2) RequestSocket returns OK with a newly connected socket.
  // 3) RequestSocket returns ERR_IO_PENDING.  The handle will be added to a
  // wait list until a socket is available to reuse or a new socket finishes
  // connecting.  |priority| will determine the placement into the wait list.
  // 4) An error occurred early on, so RequestSocket returns an error code.
  //
  // If this function returns OK, then |handle| is initialized upon return.
  // The |handle|'s is_initialized method will return true in this case.  If a
  // ClientSocket was reused, then ClientSocketPool will call
  // |handle|->set_reused(true).  In either case, the socket will have been
  // allocated and will be connected.  A client might want to know whether or
  // not the socket is reused in order to know whether or not he needs to
  // perform SSL connection or tunnel setup or to request a new socket if he
  // encounters an error with the reused socket.
  //
  // If ERR_IO_PENDING is returned, then the callback will be used to notify the
  // client of completion.
  //
  int RequestSocket(const std::string& group_name,
                    const std::string& host,
                    int port,
                    int priority,
                    ClientSocketHandle* handle,
                    CompletionCallback* callback);

  // Called to cancel a RequestSocket call that returned ERR_IO_PENDING.  The
  // same handle parameter must be passed to this method as was passed to the
  // RequestSocket call being cancelled.  The associated CompletionCallback is
  // not run.
  void CancelRequest(const std::string& group_name,
                     const ClientSocketHandle* handle);

  // Called to release a socket once the socket is no longer needed.  If the
  // socket still has an established connection, then it will be added to the
  // set of idle sockets to be used to satisfy future RequestSocket calls.
  // Otherwise, the ClientSocket is destroyed.
  void ReleaseSocket(const std::string& group_name, ClientSocket* socket);

  // Called to close any idle connections held by the connection manager.
  void CloseIdleSockets();

  // The total number of idle sockets in the pool.
  int idle_socket_count() const {
    return idle_socket_count_;
  }

  // The total number of idle sockets in a connection group.
  int IdleSocketCountInGroup(const std::string& group_name) const;

  // Determine the LoadState of a connecting ClientSocketHandle.
  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const;

 private:
  friend class base::RefCounted<ClientSocketPool>;

  // A Request is allocated per call to RequestSocket that results in
  // ERR_IO_PENDING.
  struct Request {
    ClientSocketHandle* handle;
    CompletionCallback* callback;
    int priority;
    std::string host;
    int port;
    LoadState load_state;
  };

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

  // ConnectingSocket handles the host resolution necessary for socket creation
  // and the Connect().
  class ConnectingSocket {
   public:
    enum State {
      STATE_RESOLVE_HOST,
      STATE_CONNECT
    };

    ConnectingSocket(const std::string& group_name,
                     const ClientSocketHandle* handle,
                     ClientSocketFactory* client_socket_factory,
                     ClientSocketPool* pool);
    ~ConnectingSocket();

    // Begins the host resolution and the TCP connect.  Returns OK on success,
    // in which case |callback| is not called.  On pending IO, Connect returns
    // ERR_IO_PENDING and runs |callback| on completion.
    int Connect(const std::string& host,
                int port,
                CompletionCallback* callback);

    // If Connect() returns OK, ClientSocketPool may invoke this method to get
    // the ConnectingSocket to release |socket_| to be set into the
    // ClientSocketHandle immediately.
    ClientSocket* ReleaseSocket();

    // Called by the ClientSocketPool to cancel this ConnectingSocket.  Only
    // necessary if a ClientSocketHandle is reused.
    void Cancel();

   private:
    void OnIOComplete(int result);

    const std::string group_name_;
    const ClientSocketHandle* const handle_;
    ClientSocketFactory* const client_socket_factory_;
    CompletionCallbackImpl<ConnectingSocket> callback_;
    scoped_ptr<ClientSocket> socket_;
    scoped_refptr<ClientSocketPool> pool_;
    HostResolver resolver_;
    AddressList addresses_;
    bool canceled_;

    // The time the Connect() method was called (if it got called).
    base::Time connect_start_time_;

    DISALLOW_COPY_AND_ASSIGN(ConnectingSocket);
  };

  ~ClientSocketPool();

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

  ClientSocketFactory* const client_socket_factory_;

  GroupMap group_map_;

  std::map<const ClientSocketHandle*, ConnectingSocket*> connecting_socket_map_;

  // Timer used to periodically prune idle sockets that timed out or can't be
  // reused.
  base::RepeatingTimer<ClientSocketPool> timer_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // The maximum number of sockets kept per group.
  const int max_sockets_per_group_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPool);
};

}  // namespace net

#endif  // NET_BASE_CLIENT_SOCKET_POOL_H_
