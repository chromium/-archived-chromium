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

#ifndef NET_HTTP_HTTP_CONNECTION_H_
#define NET_HTTP_HTTP_CONNECTION_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/client_socket.h"
#include "net/base/completion_callback.h"

namespace net {

class HttpConnectionManager;

// A container for a ClientSocket, representing a HTTP connection.
//
// The connection's |group_name| uniquely identifies the origin and type of the
// connection.  It is used by the HttpConnectionManager to group similar http
// connection objects.
//
// A connection object is initialized with a null socket.  It is the consumer's
// job to initialize a ClientSocket object and set it on the connection.
//
class HttpConnection {
 public:
  HttpConnection(HttpConnectionManager* mgr);
  ~HttpConnection();

  // Initializes a HttpConnection object, which involves talking to the
  // HttpConnectionManager to locate a socket to possibly reuse.
  //
  // If this method succeeds, then the socket member will be set to an existing
  // socket if an existing socket was available to reuse.  Otherwise, the
  // consumer should set the socket member of this connection object.
  //
  // This method returns ERR_IO_PENDING if it cannot complete synchronously, in
  // which case the consumer should wait for the completion callback to run.
  //
  // Init may be called multiple times.
  //
  int Init(const std::string& group_name, CompletionCallback* callback);

  // An initialized connection can be reset, which causes it to return to the
  // un-initialized state.  This releases the underlying socket, which in the
  // case of a socket that is not closed, indicates that the socket may be kept
  // alive for use by a subsequent HttpConnection.  NOTE: To prevent the socket
  // from being kept alive, be sure to call its Close method.
  void Reset();

  // Returns true when Init has completed successfully.
  bool is_initialized() const { return socket_ != NULL; }

  // These may only be used if is_initialized() is true.
  ClientSocket* socket() { return socket_->get(); }
  void set_socket(ClientSocket* s) { socket_->reset(s); }

 private:
  scoped_refptr<HttpConnectionManager> mgr_;
  scoped_ptr<ClientSocket>* socket_;
  std::string group_name_;

  DISALLOW_EVIL_CONSTRUCTORS(HttpConnection);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CONNECTION_H_
