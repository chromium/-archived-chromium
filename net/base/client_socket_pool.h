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

#ifndef NET_BASE_CLIENT_SOCKET_POOL_H_
#define NET_BASE_CLIENT_SOCKET_POOL_H_

#include <deque>
#include <map>
#include <string>

#include "base/ref_counted.h"
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
class ClientSocketPool : public base::RefCounted<ClientSocketPool>,
                         public Task {
 public:
  explicit ClientSocketPool(int max_sockets_per_group);

  // Called to request a socket for the given handle.  There are three possible
  // results: 1) the handle will be initialized with a socket to reuse, 2) the
  // handle will be initialized without a socket such that the consumer needs
  // to supply a socket, or 3) the handle will be added to a wait list until a
  // socket is available to reuse or the opportunity to create a new socket
  // arises.  The completion callback is notified in the 3rd case.
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
  int RequestSocket(ClientSocketHandle* handle, CompletionCallback* callback);

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

 private:
  friend class base::RefCounted<ClientSocketPool>;

  typedef scoped_ptr<ClientSocket> ClientSocketPtr;

  ~ClientSocketPool();

  // Closes all idle sockets if |only_if_disconnected| is false.  Else, only
  // idle sockets that are disconnected get closed.  "Maybe" refers to the
  // fact that it doesn't close all idle sockets if |only_if_disconnected| is
  // true.
  void MaybeCloseIdleSockets(bool only_if_disconnected);

  // Called when the number of idle sockets changes.
  void IncrementIdleCount();
  void DecrementIdleCount();

  // Called via PostTask by ReleaseSocket.
  void DoReleaseSocket(const std::string& group_name, ClientSocketPtr* ptr);

  // Task implementation.  This method scans the idle sockets checking to see
  // if any have been disconnected.
  virtual void Run();

  // A Request is allocated per call to RequestSocket that results in
  // ERR_IO_PENDING.
  struct Request {
    ClientSocketHandle* handle;
    CompletionCallback* callback;
  };

  // A Group is allocated per group_name when there are idle sockets or pending
  // requests.  Otherwise, the Group object is removed from the map.
  struct Group {
    Group() : active_socket_count(0) {}
    std::deque<ClientSocketPtr*> idle_sockets;
    std::deque<Request> pending_requests;
    int active_socket_count;
  };

  typedef std::map<std::string, Group> GroupMap;
  GroupMap group_map_;

  // Timer used to periodically prune sockets that have been disconnected.
  RepeatingTimer timer_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // The maximum number of sockets kept per group.
  int max_sockets_per_group_;
};

}  // namespace net

#endif  // NET_BASE_CLIENT_SOCKET_POOL_H_
