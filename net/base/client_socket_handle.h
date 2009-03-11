// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CLIENT_SOCKET_HANDLE_H_
#define NET_BASE_CLIENT_SOCKET_HANDLE_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/client_socket.h"
#include "net/base/completion_callback.h"

namespace net {

class ClientSocketPool;

// A container for a connected ClientSocket.
//
// The handle's |group_name| uniquely identifies the origin and type of the
// connection.  It is used by the ClientSocketPool to group similar connected
// client socket objects.
//
// A handle is initialized with a null socket.  It is the consumer's job to
// initialize a ClientSocket object and set it on the handle.
//
class ClientSocketHandle {
 public:
  explicit ClientSocketHandle(ClientSocketPool* pool);
  ~ClientSocketHandle();

  // Initializes a ClientSocketHandle object, which involves talking to the
  // ClientSocketPool to locate a socket to possibly reuse.  This method
  // returns either OK or ERR_IO_PENDING.
  //
  // If this method succeeds, then the socket member will be set to an existing
  // socket if an existing socket was available to reuse.  Otherwise, the
  // consumer should set the socket member of this handle.
  //
  // This method returns ERR_IO_PENDING if it cannot complete synchronously, in
  // which case the consumer should wait for the completion callback to run.
  //
  // Init may be called multiple times.
  //
  int Init(const std::string& group_name, CompletionCallback* callback);

  // An initialized handle can be reset, which causes it to return to the
  // un-initialized state.  This releases the underlying socket, which in the
  // case of a socket that still has an established connection, indicates that
  // the socket may be kept alive for use by a subsequent ClientSocketHandle.
  //
  // NOTE: To prevent the socket from being kept alive, be sure to call its
  // Disconnect method.
  //
  void Reset();

  // Returns true when Init has completed successfully.
  bool is_initialized() const { return socket_ != NULL; }

  // These may only be used if is_initialized() is true.
  ClientSocket* socket() { return socket_->get(); }
  ClientSocket* release_socket() { return socket_->release(); }
  void set_socket(ClientSocket* s) { socket_->reset(s); }

 private:
  friend class ClientSocketPool;

  scoped_refptr<ClientSocketPool> pool_;
  scoped_ptr<ClientSocket>* socket_;
  std::string group_name_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketHandle);
};

}  // namespace net

#endif  // NET_BASE_CLIENT_SOCKET_HANDLE_H_
