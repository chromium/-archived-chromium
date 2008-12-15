// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TCP/IP server that handles IO asynchronously in the specified MessageLoop.
// These objects are NOT thread safe.  They use WSAEVENT handles to monitor
// activity in a given MessageLoop.  This means that callbacks will
// happen in that loop's thread always and that all other methods (including
// constructors and destructors) should also be called from the same thread.

#ifndef NET_BASE_SOCKET_H_
#define NET_BASE_SOCKET_H_

#if defined(OS_WIN)
#include <winsock2.h>
#endif
#include <string>
#if defined(OS_WIN)
#include "base/object_watcher.h"
#elif defined(OS_POSIX)
#include "base/message_loop.h"
#include "net/base/net_util.h"
#include "net/base/net_errors.h"
#endif

#include "base/basictypes.h"
#include "base/ref_counted.h"

#if defined(OS_POSIX)
struct event;  // From libevent
#define SOCKET int
#endif

// Implements a raw socket interface
class ListenSocket : public base::RefCountedThreadSafe<ListenSocket>,
#if defined(OS_WIN)
                     public base::ObjectWatcher::Delegate
#elif defined(OS_POSIX)
                     public MessageLoopForIO::Watcher
#endif
{
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

  virtual void Listen();
  virtual void Accept();
  virtual void Read();
  virtual void Close();
  virtual void CloseSocket(SOCKET s);

  enum WaitState {
    NOT_WAITING      = 0,
    WAITING_ACCEPT   = 1,
    WAITING_READ     = 3,
    WAITING_CLOSE    = 4
  };
  // Pass any value in case of Windows, because in Windows
  // we are not using state.
  void WatchSocket(WaitState state);
  void UnwatchSocket();

#if defined(OS_WIN)
  // ObjectWatcher delegate
  virtual void OnObjectSignaled(HANDLE object);
  base::ObjectWatcher watcher_;
  HANDLE socket_event_;
#elif defined(OS_POSIX)
  WaitState wait_state_;
  // The socket's libevent wrapper
  MessageLoopForIO::FileDescriptorWatcher watcher_;
  // Called by MessagePumpLibevent when the socket is ready to do I/O
  void OnFileCanReadWithoutBlocking(int fd);
  void OnFileCanWriteWithoutBlocking(int fd);
#endif

  SOCKET socket_;
  ListenSocketDelegate *socket_delegate_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ListenSocket);
};

#endif  // NET_BASE_SOCKET_H_
