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

// winsock2.h must be included first in order to ensure it is included before
// windows.h.
#include <winsock2.h>

#include "net/base/listen_socket.h"

#define READ_BUF_SIZE 200

ListenSocket::ListenSocket(SOCKET s, ListenSocketDelegate *del)
    : socket_(s),
      socket_delegate_(del) {
  socket_event_ = WSACreateEvent();
  WSAEventSelect(socket_, socket_event_, FD_ACCEPT | FD_CLOSE | FD_READ);
  watcher_.StartWatching(socket_event_, this);
}

ListenSocket::~ListenSocket() {
  if (socket_event_) {
    watcher_.StopWatching();
    WSACloseEvent(socket_event_);
  }
  if (socket_)
    closesocket(socket_);
}

SOCKET ListenSocket::Listen(std::string ip, int port) {
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s != INVALID_SOCKET) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);
    if (bind(s, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr))) {
      closesocket(s);
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
  // TODO(erikkay): handle error
}

SOCKET ListenSocket::Accept(SOCKET s) {
  sockaddr_in from;
  int from_len = sizeof(from);
  SOCKET conn = accept(s, reinterpret_cast<SOCKADDR*>(&from), &from_len);
  if (conn != INVALID_SOCKET) {
    // a non-blocking socket
    unsigned long no_block = 1;
    ioctlsocket(conn, FIONBIO, &no_block);
  }
  return conn;
}

void ListenSocket::Accept() {
  SOCKET conn = Accept(socket_);
  if (conn == INVALID_SOCKET) {
    // TODO
  } else {
    scoped_refptr<ListenSocket> sock =
        new ListenSocket(conn, socket_delegate_);
    // it's up to the delegate to AddRef if it wants to keep it around
    socket_delegate_->DidAccept(this, sock);
  }
}

void ListenSocket::Read() {
  char buf[READ_BUF_SIZE+1];
  int len;
  do {
    len = recv(socket_, buf, READ_BUF_SIZE, 0);
    if (len == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
        break;
      } else {
        // TODO - error
        break;
      }
    } else if (len == 0) {
      // socket closed, ignore
    } else {
      // TODO(erikkay): maybe change DidRead to take a length instead
      DCHECK(len > 0 && len <= READ_BUF_SIZE);
      buf[len] = 0;
      socket_delegate_->DidRead(this, buf);
    }
  } while (len == READ_BUF_SIZE);
}

void ListenSocket::Close() {
  socket_delegate_->DidClose(this);
}

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

void ListenSocket::SendInternal(const char* bytes, int len) {
  int sent = send(socket_, bytes, len, 0);
  if (sent == SOCKET_ERROR) {
    // TODO
  } else if (sent != len) {
    // TODO
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
