// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_libevent.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "third_party/libevent/event.h"


namespace net {

namespace {

const int kInvalidSocket = -1;

// Return 0 on success, -1 on failure.
// Too small a function to bother putting in a library?
int SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags)
      return flags;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Convert values from <errno.h> to values from "net/base/net_errors.h"
int MapPosixError(int err) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (err) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return ERR_IO_PENDING;
    case ENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case ETIMEDOUT:
      return ERR_TIMED_OUT;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive
      return ERR_CONNECTION_RESET;
    case ECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case ECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case EHOSTUNREACH:
    case ENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case EADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case 0:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TCPClientSocketLibevent::TCPClientSocketLibevent(const AddressList& addresses)
    : socket_(kInvalidSocket),
      addresses_(addresses),
      current_ai_(addresses_.head()),
      waiting_connect_(false),
      read_watcher_(this),
      write_watcher_(this),
      read_callback_(NULL),
      write_callback_(NULL) {
}

TCPClientSocketLibevent::~TCPClientSocketLibevent() {
  Disconnect();
}

int TCPClientSocketLibevent::Connect(CompletionCallback* callback) {
  // If already connected, then just return OK.
  if (socket_ != kInvalidSocket)
    return OK;

  DCHECK(!waiting_connect_);

  TRACE_EVENT_BEGIN("socket.connect", this, "");
  const addrinfo* ai = current_ai_;
  DCHECK(ai);

  int rv = CreateSocket(ai);
  if (rv != OK)
    return rv;

  if (!HANDLE_EINTR(connect(socket_, ai->ai_addr,
                            static_cast<int>(ai->ai_addrlen)))) {
    TRACE_EVENT_END("socket.connect", this, "");
    // Connected without waiting!
    return OK;
  }

  // Synchronous operation not supported
  DCHECK(callback);

  if (errno != EINPROGRESS) {
    DLOG(INFO) << "connect failed: " << errno;
    close(socket_);
    socket_ = kInvalidSocket;
    return MapPosixError(errno);
  }

  // Initialize write_socket_watcher_ and link it to our MessagePump.
  // POLLOUT is set if the connection is established.
  // POLLIN is set if the connection fails.
  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_WRITE, &write_socket_watcher_,
          &write_watcher_)) {
    DLOG(INFO) << "WatchFileDescriptor failed: " << errno;
    close(socket_);
    socket_ = kInvalidSocket;
    return MapPosixError(errno);
  }

  waiting_connect_ = true;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

void TCPClientSocketLibevent::Disconnect() {
  if (socket_ == kInvalidSocket)
    return;

  TRACE_EVENT_INSTANT("socket.disconnect", this, "");

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  close(socket_);
  socket_ = kInvalidSocket;
  waiting_connect_ = false;

  // Reset for next time.
  current_ai_ = addresses_.head();
}

bool TCPClientSocketLibevent::IsConnected() const {
  if (socket_ == kInvalidSocket || waiting_connect_)
    return false;

  // Check if connection is alive.
  char c;
  int rv = HANDLE_EINTR(recv(socket_, &c, 1, MSG_PEEK));
  if (rv == 0)
    return false;
  if (rv == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

bool TCPClientSocketLibevent::IsConnectedAndIdle() const {
  if (socket_ == kInvalidSocket || waiting_connect_)
    return false;

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = HANDLE_EINTR(recv(socket_, &c, 1, MSG_PEEK));
  if (rv >= 0)
    return false;
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

int TCPClientSocketLibevent::Read(IOBuffer* buf,
                                  int buf_len,
                                  CompletionCallback* callback) {
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect_);
  DCHECK(!read_callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK_GT(buf_len, 0);

  TRACE_EVENT_BEGIN("socket.read", this, "");
  int nread = HANDLE_EINTR(read(socket_, buf->data(), buf_len));
  if (nread >= 0) {
    TRACE_EVENT_END("socket.read", this, StringPrintf("%d bytes", nread));
    return nread;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    DLOG(INFO) << "read failed, errno " << errno;
    return MapPosixError(errno);
  }

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    DLOG(INFO) << "WatchFileDescriptor failed on read, errno " << errno;
    return MapPosixError(errno);
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPClientSocketLibevent::Write(IOBuffer* buf,
                                   int buf_len,
                                   CompletionCallback* callback) {
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect_);
  DCHECK(!write_callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK_GT(buf_len, 0);

  TRACE_EVENT_BEGIN("socket.write", this, "");
  int nwrite = HANDLE_EINTR(write(socket_, buf->data(), buf_len));
  if (nwrite >= 0) {
    TRACE_EVENT_END("socket.write", this, StringPrintf("%d bytes", nwrite));
    return nwrite;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return MapPosixError(errno);

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    DLOG(INFO) << "WatchFileDescriptor failed on write, errno " << errno;
    return MapPosixError(errno);
  }


  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPClientSocketLibevent::CreateSocket(const addrinfo* ai) {
  socket_ = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (socket_ == kInvalidSocket)
    return MapPosixError(errno);

  if (SetNonBlocking(socket_))
    return MapPosixError(errno);

  return OK;
}

void TCPClientSocketLibevent::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(read_callback_);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback* c = read_callback_;
  read_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketLibevent::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketLibevent::DidCompleteConnect() {
  int result = ERR_UNEXPECTED;

  TRACE_EVENT_END("socket.connect", this, "");

  // Check to see if connect succeeded
  int error_code = 0;
  socklen_t len = sizeof(error_code);
  if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error_code, &len) < 0)
    error_code = errno;

  if (error_code == EINPROGRESS || error_code == EALREADY) {
    NOTREACHED();  // This indicates a bug in libevent or our code.
    result = ERR_IO_PENDING;
  } else if (current_ai_->ai_next && (
             error_code == EADDRNOTAVAIL ||
             error_code == EAFNOSUPPORT ||
             error_code == ECONNREFUSED ||
             error_code == ENETUNREACH ||
             error_code == EHOSTUNREACH ||
             error_code == ETIMEDOUT)) {
    // This address failed, try next one in list.
    const addrinfo* next = current_ai_->ai_next;
    Disconnect();
    current_ai_ = next;
    result = Connect(write_callback_);
  } else {
    result = MapPosixError(error_code);
    bool ok = write_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    waiting_connect_ = false;
  }

  if (result != ERR_IO_PENDING) {
    DoWriteCallback(result);
  }
}

void TCPClientSocketLibevent::DidCompleteRead() {
  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(read(socket_, read_buf_->data(),
                                        read_buf_len_));

  int result;
  if (bytes_transferred >= 0) {
    TRACE_EVENT_END("socket.read", this,
                    StringPrintf("%d bytes", bytes_transferred));
    result = bytes_transferred;
  } else {
    result = MapPosixError(errno);
  }

  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    DoReadCallback(result);
  }
}

void TCPClientSocketLibevent::DidCompleteWrite() {
  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(write(socket_, write_buf_->data(),
                                         write_buf_len_));

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    TRACE_EVENT_END("socket.write", this,
                    StringPrintf("%d bytes", bytes_transferred));
  } else {
    result = MapPosixError(errno);
  }

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    write_socket_watcher_.StopWatchingFileDescriptor();
    DoWriteCallback(result);
  }
}

int TCPClientSocketLibevent::GetPeerName(struct sockaddr *name,
                                         socklen_t *namelen) {
  return ::getpeername(socket_, name, namelen);
}

}  // namespace net
