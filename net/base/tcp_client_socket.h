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
#include <sys/socket.h>  // for struct sockaddr
#define SOCKET int
#include "base/message_loop.h"
#endif

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "net/base/address_list.h"
#include "net/base/client_socket.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"

namespace net {

// A client socket that uses TCP as the transport layer.
//
// NOTE: The windows implementation supports half duplex only.
// Read and Write calls must not be in progress at the same time.
// The libevent implementation supports full duplex because that
// made it slightly easier to implement ssl.
class TCPClientSocket : public ClientSocket,
#if defined(OS_WIN)
                        public base::ObjectWatcher::Delegate
#elif defined(OS_POSIX)
                        public MessageLoopForIO::Watcher
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
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is not supported
  // on Windows (but is supported on Linux and Mac for ease of implementation
  // of SSLClientSocket)
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(const char* buf, int buf_len, CompletionCallback* callback);

#if defined(OS_POSIX)
  // Identical to posix system call of same name
  // Needed by ssl_client_socket_nss
  virtual int GetPeerName(struct sockaddr *name, socklen_t *namelen);
#endif

 private:
  SOCKET socket_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list, or NULL if all addrinfos have been tried.
  const struct addrinfo* current_ai_;

#if defined(OS_WIN)
  enum WaitState {
    NOT_WAITING,
    WAITING_CONNECT,
    WAITING_READ,
    WAITING_WRITE
  };
  WaitState wait_state_;

  // base::ObjectWatcher::Delegate methods:
  virtual void OnObjectSignaled(HANDLE object);

  OVERLAPPED overlapped_;
  WSABUF buffer_;

  base::ObjectWatcher watcher_;

  void DidCompleteIO();
#elif defined(OS_POSIX)
  // Whether we're currently waiting for connect() to complete
  bool waiting_connect_;

  // The socket's libevent wrapper
  MessageLoopForIO::FileDescriptorWatcher socket_watcher_;

  // Called by MessagePumpLibevent when the socket is ready to do I/O
  void OnFileCanReadWithoutBlocking(int fd);
  void OnFileCanWriteWithoutBlocking(int fd);

  // The buffer used by OnSocketReady to retry Read requests
  char* buf_;
  int buf_len_;

  // The buffer used by OnSocketReady to retry Write requests
  const char* write_buf_;
  int write_buf_len_;

  // External callback; called when write is complete.
  CompletionCallback* write_callback_;

  void DoWriteCallback(int rv);
  void DidCompleteRead();
  void DidCompleteWrite();
#endif

  // External callback; called when read (and on Windows, write) is complete.
  CompletionCallback* callback_;

  int CreateSocket(const struct addrinfo* ai);
  void DoCallback(int rv);
  void DidCompleteConnect();
};

// Tiny helper class to do a synchronous connect,
// in lieu of directly supporting that in TcpClientSocket.
// This avoids cluttering the main codepath with code only used by unit tests.
// TODO(dkegel): move this to its own header file.
class TCPClientSocketSyncConnector
    : public base::RefCounted<TCPClientSocketSyncConnector> {
 public:
  // Connect given socket synchronously.
  // Returns network error code.
  static int Connect(net::TCPClientSocket* sock) {
    // Start up a throwaway IO thread just for this.
    // TODO(port): use some existing thread pool instead?
    base::Thread io_thread("SyncConnect");
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread.StartWithOptions(options);

    // Post a request to do the connect on that thread.
    scoped_refptr<TCPClientSocketSyncConnector> connector =
        new TCPClientSocketSyncConnector(sock);
    io_thread.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(connector.get(),
        &net::TCPClientSocketSyncConnector::DoConnect));
    connector->Wait();
    return connector->GetError();
  }

 private:
  // Start a connect.  Must be called on an IO thread.
  void DoConnect() {
    net_error_ = sock_->Connect(&connect_callback_);
    if (net_error_ != ERR_IO_PENDING)
      event_.Signal();
  }

  // Callback called on same IO thread when connection complete.
  void ConnectDone(int rv) {
    net_error_ = rv;
    event_.Signal();
  }

  // Call this after posting a call to DoConnect().
  void Wait() { event_.Wait(); }

  // Call this after Wait() if you need the final error code from the connect.
  int GetError() { return net_error_; }

  // sock is owned by caller, but must remain valid while this object lives.
  explicit TCPClientSocketSyncConnector(TCPClientSocket* sock) :
      event_(false, false),
      sock_(sock),
      net_error_(0),
      connect_callback_(this, &net::TCPClientSocketSyncConnector::ConnectDone) {
  }

  base::WaitableEvent event_;
  net::TCPClientSocket* sock_;
  int net_error_;
  net::CompletionCallbackImpl<TCPClientSocketSyncConnector> connect_callback_;
  DISALLOW_COPY_AND_ASSIGN(TCPClientSocketSyncConnector);
};

}  // namespace net

#endif  // NET_BASE_TCP_CLIENT_SOCKET_H_

