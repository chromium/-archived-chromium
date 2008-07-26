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

#ifndef NET_HTTP_HTTP_CONNECTION_MANAGER_H_
#define NET_HTTP_HTTP_CONNECTION_MANAGER_H_

#include <deque>
#include <map>

#include "base/ref_counted.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"

namespace net {

class ClientSocket;

// A HttpConnectionManager is used to restrict the number of HTTP sockets
// open at a time.  It also maintains a list of idle persistent sockets.
//
// The HttpConnectionManager allocates SocketHandle objects, but it is not
// responsible for allocating the associated ClientSocket object.  The
// consumer must do so if it gets a SocketHandle with a null ClientSocket.
//
class HttpConnectionManager : public base::RefCounted<HttpConnectionManager>,
                              public Task {
 public:
  HttpConnectionManager();

  // The maximum number of simultaneous sockets per group.
  enum { kMaxSocketsPerGroup = 6 };

  typedef scoped_ptr<ClientSocket> SocketHandle;

  // Called to get access to a SocketHandle object for the given group name.
  //
  // If this function returns OK, then |*handle| will reference a SocketHandle
  // object.  If ERR_IO_PENDING is returned, then the completion callback will
  // be called when |*handle| has been populated.  Otherwise, an error code is
  // returned.
  //
  // If the resultant SocketHandle object has a null member, then it is the
  // callers job to create a ClientSocket and associate it with the handle.
  //
  int RequestSocket(const std::string& group_name, SocketHandle** handle,
                    CompletionCallback* callback);

  // Called to cancel a RequestSocket call that returned ERR_IO_PENDING.  The
  // same group_name and handle parameters must be passed to this method as
  // were passed to the RequestSocket call being cancelled.  The associated
  // CompletionCallback is not run.
  void CancelRequest(const std::string& group_name, SocketHandle** handle);

  // Called to release a SocketHandle object that is no longer in use.  If the
  // handle has a ClientSocket that is still connected, then this handle may be
  // added to the keep-alive set of sockets.
  void ReleaseSocket(const std::string& group_name, SocketHandle* handle);

  // Called to close any idle connections held by the connection manager.
  void CloseIdleSockets();

 private:
  friend class base::RefCounted<HttpConnectionManager>;

  ~HttpConnectionManager();

  // Closes all idle sockets if |only_if_disconnected| is false.  Else, only
  // idle sockets that are disconnected get closed.
  void MaybeCloseIdleSockets(bool only_if_disconnected);

  // Called when the number of idle sockets changes.
  void IncrementIdleCount();
  void DecrementIdleCount();

  // Called via PostTask by ReleaseSocket.
  void DoReleaseSocket(const std::string& group_name, SocketHandle* handle);

  // Task implementation.  This method scans the idle sockets checking to see
  // if any have been disconnected.
  virtual void Run();

  // A Request is allocated per call to RequestSocket that results in
  // ERR_IO_PENDING.
  struct Request {
    SocketHandle** result;
    CompletionCallback* callback;
  };

  // A Group is allocated per group_name when there are idle sockets or pending
  // requests.  Otherwise, the Group object is removed from the map.
  struct Group {
    std::deque<SocketHandle*> idle_sockets;
    std::deque<Request> pending_requests;
    int active_socket_count;
    Group() : active_socket_count(0) {}
  };

  typedef std::map<std::string, Group> GroupMap;
  GroupMap group_map_;

  // Timer used to periodically prune sockets that have been disconnected.
  RepeatingTimer timer_;

  // The total number of idle sockets in the system.
  int idle_count_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CONNECTION_MANAGER_H_
