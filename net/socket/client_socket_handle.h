// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
#define NET_SOCKET_CLIENT_SOCKET_HANDLE_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/load_states.h"
#include "net/socket/client_socket.h"

namespace net {

class ClientSocketPool;

// A container for a ClientSocket.
//
// The handle's |group_name| uniquely identifies the origin and type of the
// connection.  It is used by the ClientSocketPool to group similar connected
// client socket objects.
//
class ClientSocketHandle {
 public:
  explicit ClientSocketHandle(ClientSocketPool* pool);
  ~ClientSocketHandle();

  // Initializes a ClientSocketHandle object, which involves talking to the
  // ClientSocketPool to obtain a connected socket, possibly reusing one.  This
  // method returns either OK or ERR_IO_PENDING.  On ERR_IO_PENDING, |priority|
  // is used to determine the placement in ClientSocketPool's wait list.
  //
  // If this method succeeds, then the socket member will be set to an existing
  // connected socket if an existing connected socket was available to reuse,
  // otherwise it will be set to a new connected socket.  Consumers can then
  // call is_reused() to see if the socket was reused.  If not reusing an
  // existing socket, ClientSocketPool may need to establish a new
  // connection to the |resolve_info.host| |resolve_info.port| pair.
  //
  // This method returns ERR_IO_PENDING if it cannot complete synchronously, in
  // which case the consumer will be notified of completion via |callback|.
  //
  // Init may be called multiple times.
  //
  int Init(const std::string& group_name,
           const HostResolver::RequestInfo& resolve_info,
           int priority,
           CompletionCallback* callback);

  // An initialized handle can be reset, which causes it to return to the
  // un-initialized state.  This releases the underlying socket, which in the
  // case of a socket that still has an established connection, indicates that
  // the socket may be kept alive for use by a subsequent ClientSocketHandle.
  //
  // NOTE: To prevent the socket from being kept alive, be sure to call its
  // Disconnect method.  This will result in the ClientSocketPool deleting the
  // ClientSocket.
  void Reset();

  // Used after Init() is called, but before the ClientSocketPool has
  // initialized the ClientSocketHandle.
  LoadState GetLoadState() const;

  // Returns true when Init() has completed successfully.
  bool is_initialized() const { return socket_ != NULL; }

  // Used by ClientSocketPool to initialize the ClientSocketHandle.
  void set_is_reused(bool is_reused) { is_reused_ = is_reused; }
  void set_socket(ClientSocket* s) { socket_.reset(s); }

  // These may only be used if is_initialized() is true.
  const std::string& group_name() const { return group_name_; }
  ClientSocket* socket() { return socket_.get(); }
  ClientSocket* release_socket() { return socket_.release(); }
  bool is_reused() const { return is_reused_; }

 private:
  // Called on asynchronous completion of an Init() request.
  void OnIOComplete(int result);

  // Called on completion (both asynchronous & synchronous) of an Init()
  // request.
  void HandleInitCompletion(int result);

  // Resets the state of the ClientSocketHandle.  |cancel| indicates whether or
  // not to try to cancel the request with the ClientSocketPool.
  void ResetInternal(bool cancel);

  scoped_refptr<ClientSocketPool> pool_;
  scoped_ptr<ClientSocket> socket_;
  std::string group_name_;
  bool is_reused_;
  CompletionCallbackImpl<ClientSocketHandle> callback_;
  CompletionCallback* user_callback_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketHandle);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_HANDLE_H_
