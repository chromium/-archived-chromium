// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_CLIENT_SOCKET_H_
#define NET_BASE_TCP_CLIENT_SOCKET_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include "base/object_watcher.h"
#elif defined(OS_POSIX)
struct event;  // From libevent
#define SOCKET int
#include "base/message_pump_libevent.h"
#endif

#include "base/completion_callback.h"
#include "base/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/client_socket.h"

namespace net {

// A client socket that uses TCP as the transport layer.
//
// NOTE: The implementation supports half duplex only.  Read and Write calls
// must not be in progress at the same time.
class TCPClientSocket : public ClientSocket,
#if defined(OS_WIN)
                        public base::ObjectWatcher::Delegate 
#elif defined(OS_POSIX)
                        public base::MessagePumpLibevent::Watcher
#endif
{
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  explicit TCPClientSocket(const AddressList& addresses);

  ~TCPClientSocket();

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual int ReconnectIgnoringLastError(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;

  // Socket methods:
  // Try to transfer buf_len bytes to/from socket.
  // If a result is available now, return it; else call back later with one.
  // Do not call again until a result is returned!
  // If any bytes were transferred, the result is the byte count.
  // On error, result is a negative error code; see net/base/net_error_list.h 
  // TODO: what would a zero return value indicate?
  // TODO: support multiple outstanding requests?
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(const char* buf, int buf_len, CompletionCallback* callback);

 private:
  SOCKET socket_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list, or NULL if all addrinfos have been tried.
  const struct addrinfo* current_ai_;

  enum WaitState {
    NOT_WAITING,
    WAITING_CONNECT,
    WAITING_READ,
    WAITING_WRITE
  };
  WaitState wait_state_;

#if defined(OS_WIN)
  // base::ObjectWatcher::Delegate methods:
  virtual void OnObjectSignaled(HANDLE object);

  OVERLAPPED overlapped_;
  WSABUF buffer_;

  base::ObjectWatcher watcher_;
#elif defined(OS_POSIX)
  // The socket's libevent wrapper
  scoped_ptr<event> event_;

  // Called by MessagePumpLibevent when the socket is ready to do I/O
  void OnSocketReady(short flags);

  // The buffer used by OnSocketReady to retry Read and Write requests
  char* buf_;
  int buf_len_;
#endif

  // External callback; called when read or write is complete.
  CompletionCallback* callback_;

  int CreateSocket(const struct addrinfo* ai);
  void DoCallback(int rv);
  void DidCompleteConnect();
  void DidCompleteIO();
};

}  // namespace net

#endif  // NET_BASE_TCP_CLIENT_SOCKET_H_

