// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
// winsock2.h must be included first in order to ensure it is included before
// windows.h.
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "net/base/net_errors.h"
#include "third_party/libevent/event.h"
#endif

#include "net/base/net_util.h"
#include "net/base/listen_socket.h"

#if defined(OS_WIN)
#define socklen_t int
#elif defined(OS_POSIX)
const int INVALID_SOCKET = -1; // Used same name as in Windows to avoid #ifdef
const int SOCKET_ERROR = -1;
#endif

const int kReadBufSize = 200;

ListenSocket::ListenSocket(SOCKET s, ListenSocketDelegate *del)
    : socket_(s),
      socket_delegate_(del) {
#if defined(OS_WIN)
  socket_event_ = WSACreateEvent();
  // TODO(ibrar): error handling in case of socket_event_ == WSA_INVALID_EVENT
  WatchSocket(NOT_WAITING);
#endif
}

ListenSocket::~ListenSocket() {
#if defined(OS_WIN)
  if (socket_event_) {
    WSACloseEvent(socket_event_);
    socket_event_ = WSA_INVALID_EVENT;
  }
#endif
  CloseSocket(socket_);
}

SOCKET ListenSocket::Listen(std::string ip, int port) {
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s != INVALID_SOCKET) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
#if defined(OS_WIN)
      closesocket(s);
#elif defined(OS_POSIX)
      close(s);
#endif
      s = INVALID_SOCKET;
    }
  }
  return s;
}

ListenSocket* ListenSocket::Listen(std::string ip, int port,
                                   ListenSocketDelegate* del) {
  SOCKET s = Listen(ip, port);
  if (s == INVALID_SOCKET) {
    // TODO(erikkay): error handling
  } else {
    ListenSocket* sock = new ListenSocket(s, del);
    sock->Listen();
    return sock;
  }
  return NULL;
}

void ListenSocket::Listen() {
  int backlog = 10; // TODO(erikkay): maybe don't allow any backlog?
  listen(socket_, backlog);
  // TODO(erikkay): error handling
#if defined(OS_POSIX)
  WatchSocket(WAITING_ACCEPT);
#endif
}

SOCKET ListenSocket::Accept(SOCKET s) {
  sockaddr_in from;
  socklen_t from_len = sizeof(from);
  SOCKET conn = accept(s, reinterpret_cast<sockaddr*>(&from), &from_len);
  if (conn != INVALID_SOCKET) {
    net::SetNonBlocking(conn);
  }
  return conn;
}

void ListenSocket::Accept() {
  SOCKET conn = Accept(socket_);
  if (conn != INVALID_SOCKET) {
    scoped_refptr<ListenSocket> sock =
        new ListenSocket(conn, socket_delegate_);
    // it's up to the delegate to AddRef if it wants to keep it around
#if defined(OS_POSIX)
    sock->WatchSocket(WAITING_READ);
#endif
    socket_delegate_->DidAccept(this, sock);
  } else {
    // TODO(ibrar): some error handling required here
  }
}

void ListenSocket::Read() {
  char buf[kReadBufSize + 1]; // +1 for null termination
  int len;
  do {
    len = recv(socket_, buf, kReadBufSize, 0);
    if (len == SOCKET_ERROR) {
#if defined(OS_WIN)
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
#elif defined(OS_POSIX)
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
        break;
      } else {
        // TODO(ibrar): some error handling required here
        break;
      }
    } else if (len == 0) {
      // In Windows, Close() is called by OnObjectSignaled.  In POSIX, we need
      // to call it here.
#if defined(OS_POSIX)
      Close();
#endif
    } else {
      // TODO(ibrar): maybe change DidRead to take a length instead
      DCHECK(len > 0 && len <= kReadBufSize);
      buf[len] = 0; // already create a buffer with +1 length
      socket_delegate_->DidRead(this, buf);
    }
  } while (len == kReadBufSize);
}

void ListenSocket::CloseSocket(SOCKET s) {
  if (s && s != INVALID_SOCKET) {
    UnwatchSocket();
#if defined(OS_WIN)
    closesocket(s);
#elif defined(OS_POSIX)
    close(s);
#endif
  }
}

void ListenSocket::Close() {
#if defined(OS_POSIX)
  if (wait_state_ == WAITING_CLOSE)
    return;
  wait_state_ = WAITING_CLOSE;
#endif
  socket_delegate_->DidClose(this);
}

void ListenSocket::UnwatchSocket() {
#if defined(OS_WIN)
  watcher_.StopWatching();
#elif defined(OS_POSIX)
  watcher_.StopWatchingFileDescriptor();
#endif
}

void ListenSocket::WatchSocket(WaitState state) {
#if defined(OS_WIN)
  WSAEventSelect(socket_, socket_event_, FD_ACCEPT | FD_CLOSE | FD_READ);
  watcher_.StartWatching(socket_event_, this);
#elif defined(OS_POSIX)
  // Implicitly calls StartWatchingFileDescriptor().
  MessageLoopForIO::current()->WatchFileDescriptor(
      socket_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
  wait_state_ = state;
#endif
}

void ListenSocket::SendInternal(const char* bytes, int len) {
  int sent = send(socket_, bytes, len, 0);
  if (sent == SOCKET_ERROR) {
#if defined(OS_WIN)
  int err = WSAGetLastError();
  if (err == WSAEWOULDBLOCK) {
#elif defined(OS_POSIX)
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
    // TODO (ibrar): there should be logic here to handle this because
    // it is not an error
    }
  } else if (sent != len) {
    LOG(ERROR) << "send failed: ";
  }
}

void ListenSocket::Send(const char* bytes, int len, bool append_linefeed) {
  SendInternal(bytes, len);
  if (append_linefeed) {
    SendInternal("\r\n", 2);
  }
}

void ListenSocket::Send(const std::string& str, bool append_linefeed) {
  Send(str.data(), static_cast<int>(str.length()), append_linefeed);
}

// TODO (ibrar): We can add these functions into OS dependent files
#if defined(OS_WIN) 
// MessageLoop watcher callback
void ListenSocket::OnObjectSignaled(HANDLE object) {
  WSANETWORKEVENTS ev;
  if (SOCKET_ERROR == WSAEnumNetworkEvents(socket_, socket_event_, &ev)) {
    // TODO
    return;
  }
  
  // The object was reset by WSAEnumNetworkEvents.  Watch for the next signal.
  watcher_.StartWatching(object, this);
  
  if (ev.lNetworkEvents == 0) {
    // Occasionally the event is set even though there is no new data.
    // The net seems to think that this is ignorable.
    return;
  }
  if (ev.lNetworkEvents & FD_ACCEPT) {
    Accept();
  }
  if (ev.lNetworkEvents & FD_READ) {
    Read();
  }
  if (ev.lNetworkEvents & FD_CLOSE) {
    Close();
  }
}
#elif defined(OS_POSIX)
void ListenSocket::OnFileCanReadWithoutBlocking(int fd) {
  if (wait_state_ == WAITING_ACCEPT) {
    Accept();
  }
  if (wait_state_ == WAITING_READ) {
    Read();
  }
  if (wait_state_ == WAITING_CLOSE) {
    // Close() is called by Read() in the Linux case.
    // TODO(erikkay): this seems to get hit multiple times after the close
  }
}

void ListenSocket::OnFileCanWriteWithoutBlocking(int fd) {
  // MessagePumpLibevent callback, we don't listen for write events
  // so we shouldn't ever reach here.
  NOTREACHED();
}

#endif
