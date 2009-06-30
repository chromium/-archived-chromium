// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_H_

#include <deque>
#include <map>
#include <string>

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/load_states.h"

namespace net {

class ClientSocket;
class ClientSocketHandle;

// A ClientSocketPool is used to restrict the number of sockets open at a time.
// It also maintains a list of idle persistent sockets.
//
class ClientSocketPool : public base::RefCounted<ClientSocketPool> {
 public:
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
  virtual int RequestSocket(const std::string& group_name,
                            const HostResolver::RequestInfo& resolve_info,
                            int priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback) = 0;

  // Called to cancel a RequestSocket call that returned ERR_IO_PENDING.  The
  // same handle parameter must be passed to this method as was passed to the
  // RequestSocket call being cancelled.  The associated CompletionCallback is
  // not run.
  virtual void CancelRequest(const std::string& group_name,
                             const ClientSocketHandle* handle) = 0;

  // Called to release a socket once the socket is no longer needed.  If the
  // socket still has an established connection, then it will be added to the
  // set of idle sockets to be used to satisfy future RequestSocket calls.
  // Otherwise, the ClientSocket is destroyed.
  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket) = 0;

  // Called to close any idle connections held by the connection manager.
  virtual void CloseIdleSockets() = 0;

  // The total number of idle sockets in the pool.
  virtual int IdleSocketCount() const = 0;

  // The total number of idle sockets in a connection group.
  virtual int IdleSocketCountInGroup(const std::string& group_name) const = 0;

  // Determine the LoadState of a connecting ClientSocketHandle.
  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const = 0;

 protected:
  ClientSocketPool() {}
  virtual ~ClientSocketPool() {}

 private:
  friend class base::RefCounted<ClientSocketPool>;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_H_
