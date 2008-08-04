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

// TCP/IP server that handles IO asynchronously in the specified MessageLoop.
// These objects are NOT thread safe.  They use WSAEVENT handles to monitor
// activity in a given MessageLoop.  This means that callbacks will
// happen in that loop's thread always and that all other methods (including
// constructors and destructors) should also be called from the same thread.

#ifndef NET_BASE_SOCKET_H_
#define NET_BASE_SOCKET_H_

#include "base/basictypes.h"
#include "base/object_watcher.h"
#include "base/ref_counted.h"

#include <winsock2.h>

// Implements a raw socket interface
class ListenSocket : public base::RefCountedThreadSafe<ListenSocket>,
                     public base::ObjectWatcher::Delegate {
 public:
  // TODO(erikkay): this delegate should really be split into two parts
  // to split up the listener from the connected socket.  Perhaps this class
  // should be split up similarly.
  class ListenSocketDelegate {
   public:
    // server is the original listening Socket, connection is the new
    // Socket that was created.  Ownership of connection is transferred
    // to the delegate with this call.
    virtual void DidAccept(ListenSocket *server, ListenSocket *connection) = 0;
    virtual void DidRead(ListenSocket *connection, const std::string& data) = 0;
    virtual void DidClose(ListenSocket *sock) = 0;
  };

  // Listen on port for the specified IP address.  Use 127.0.0.1 to only
  // accept local connections.
  static ListenSocket* Listen(std::string ip, int port,
                              ListenSocketDelegate* del);
  virtual ~ListenSocket();

  // send data to the socket
  void Send(const char* bytes, int len, bool append_linefeed = false);
  void Send(const std::string& str, bool append_linefeed = false);

 protected:
  ListenSocket(SOCKET s, ListenSocketDelegate* del);
  static SOCKET Listen(std::string ip, int port);
  // if valid, returned SOCKET is non-blocking
  static SOCKET Accept(SOCKET s);

  virtual void SendInternal(const char* bytes, int len);

  // ObjectWatcher delegate
  virtual void OnObjectSignaled(HANDLE object);

  virtual void Listen();
  virtual void Accept();
  virtual void Read();
  virtual void Close();

  SOCKET socket_;
  HANDLE socket_event_;
  ListenSocketDelegate *socket_delegate_;
  
  base::ObjectWatcher watcher_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ListenSocket);
};

#endif // NET_BASE_SOCKET_H_
