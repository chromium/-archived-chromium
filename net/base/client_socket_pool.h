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
#include "net/base/completion_callback.h"

namespace net {

class ClientSocket;
class ClientSocketHandle;

// A ClientSocketPool is used to restrict the number of sockets open at a time.
// It also maintains a list of idle persistent sockets.
//
// The ClientSocketPool allocates scoped_ptr<ClientSocket> objects, but it is
// not responsible for allocating the associated ClientSocket objects.  The
// consumer must do so if it gets a scoped_ptr<ClientSocket> with a null value.
//
class ClientSocketPool : public base::RefCounted<ClientSocketPool> {
 public:
  explicit ClientSocketPool(int max_sockets_per_group);

  // Called to request a socket for the given handle.  There are three possible
  // results: 1) the handle will be initialized with a socket to reuse, 2) the
  // handle will be initialized without a socket such that the consumer needs
  // to supply a socket, or 3) the handle will be added to a wait list until a
  // socket is available to reuse or the opportunity to create a new socket
  // arises.  The completion callback is notified in the 3rd case.  |priority|
  // will determine the placement into the wait list.
  //
  // If this function returns OK, then |handle| is initialized upon return.
  // The |handle|'s is_initialized method will return true in this case.  If a
  // ClientSocket was reused, then |handle|'s socket member will be non-NULL.
  // Otherwise, the consumer will need to supply |handle| with a socket by
  // allocating a new ClientSocket object and calling the |handle|'s set_socket
  // method.
  //
  // If ERR_IO_PENDING is returned, then the completion callback will be called
  // when |handle| has been initialized.
  //
  int RequestSocket(ClientSocketHandle* handle,
                    int priority,
                    CompletionCallback* callback);

  // Called to cancel a RequestSocket call that returned ERR_IO_PENDING.  The
  // same handle parameter must be passed to this method as was passed to the
  // RequestSocket call being cancelled.  The associated CompletionCallback is
  // not run.
  void CancelRequest(ClientSocketHandle* handle);

  // Called to release the socket member of an initialized ClientSocketHandle
  // once the socket is no longer needed.  If the socket member is non-null and
  // still has an established connection, then it will be added to the idle set
  // of sockets to be used to satisfy future RequestSocket calls.  Otherwise,
  // the ClientSocket is destroyed.
  void ReleaseSocket(ClientSocketHandle* handle);

  // Called to close any idle connections held by the connection manager.
  void CloseIdleSockets();

  // The total number of idle sockets in the pool.
  int idle_socket_count() const {
    return idle_socket_count_;
  }

 private:
  friend class base::RefCounted<ClientSocketPool>;

  typedef scoped_ptr<ClientSocket> ClientSocketPtr;

  // A Request is allocated per call to RequestSocket that results in
  // ERR_IO_PENDING.
  struct Request {
    ClientSocketHandle* handle;
    CompletionCallback* callback;
    int priority;
  };

  // Entry for a persistent socket which became idle at time |start_time|.
  struct IdleSocket {
    ClientSocketPtr* ptr;
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

  // A Group is allocated per group_name when there are idle sockets or pending
  // requests.  Otherwise, the Group object is removed from the map.
  struct Group {
    Group() : active_socket_count(0) {}
    std::deque<IdleSocket> idle_sockets;
    RequestQueue pending_requests;
    int active_socket_count;
  };

  typedef std::map<std::string, Group> GroupMap;

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
  void DoReleaseSocket(const std::string& group_name, ClientSocketPtr* ptr);

  // Called when timer_ fires.  This method scans the idle sockets removing
  // sockets that timed out or can't be reused.
  void OnCleanupTimerFired() {
    CleanupIdleSockets(false);
  }

  GroupMap group_map_;

  // Timer used to periodically prune idle sockets that timed out or can't be
  // reused.
  base::RepeatingTimer<ClientSocketPool> timer_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // The maximum number of sockets kept per group.
  int max_sockets_per_group_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPool);
};

}  // namespace net

#endif  // NET_BASE_CLIENT_SOCKET_POOL_H_
