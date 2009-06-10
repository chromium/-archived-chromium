// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClientSocketPoolBase is an implementation class for building new
// ClientSocketPools.  New ClientSocketPools should compose
// ClientSocketPoolBase.  ClientSocketPoolBase provides functionality for
// managing socket reuse and restricting the number of open sockets within a
// "group".  It always returns a connected socket.  Clients of
// ClientSocketPoolBase need to provide a ConnectingSocketFactory to generate
// ConnectingSockets that actually perform the socket connection.

#ifndef NET_BASE_CLIENT_SOCKET_POOL_BASE_H_
#define NET_BASE_CLIENT_SOCKET_POOL_BASE_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"

namespace net {

class ClientSocket;
class ClientSocketHandle;
class TCPConnectingSocket;

// A ClientSocketPoolBase is used to restrict the number of sockets open at
// a time.  It also maintains a list of idle persistent sockets.
class ClientSocketPoolBase : public base::RefCounted<ClientSocketPoolBase> {
 public:
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

  // TODO(willchan): Change the return value of CreateConnectingSocket to be
  // ConnectingSocket*, once I've extracted out an interface for it.
  class ConnectingSocketFactory {
   public:
    ConnectingSocketFactory() {}
    virtual ~ConnectingSocketFactory() {}

    // Creates a TCPConnectingSocket.  Never returns NULL.
    virtual TCPConnectingSocket* CreateConnectingSocket(
        const std::string& group_name, const Request& request) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectingSocketFactory);
  };

  ClientSocketPoolBase(
      int max_sockets_per_group,
      const ConnectingSocketFactory* connecting_socket_factory);
  ~ClientSocketPoolBase();

  int RequestSocket(const std::string& group_name, const Request& request);

  void CancelRequest(const std::string& group_name,
                     const ClientSocketHandle* handle);

  void ReleaseSocket(const std::string& group_name, ClientSocket* socket);

  int idle_socket_count() const { return idle_socket_count_; }

  int IdleSocketCountInGroup(const std::string& group_name) const;

  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const;

  // Closes all idle sockets if |force| is true.  Else, only closes idle
  // sockets that timed out or can't be reused.
  void CleanupIdleSockets(bool force);

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
  // TODO(willchan): When we decouple Requests and ConnectingSockets, this will
  // return the callback of the request we select.
  CompletionCallback* OnConnectingRequestComplete(
      const std::string& group_name,
      const ClientSocketHandle* handle,
      bool deactivate,
      ClientSocket* socket);

 private:
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

  GroupMap group_map_;

  std::map<const ClientSocketHandle*, TCPConnectingSocket*>
      connecting_socket_map_;

  // Timer used to periodically prune idle sockets that timed out or can't be
  // reused.
  base::RepeatingTimer<ClientSocketPoolBase> timer_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // The maximum number of sockets kept per group.
  const int max_sockets_per_group_;

  const ConnectingSocketFactory* const connecting_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolBase);
};

}  // namespace net

#endif  // NET_BASE_CLIENT_SOCKET_POOL_BASE_H_
