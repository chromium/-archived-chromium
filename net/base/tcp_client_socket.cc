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

#include "net/base/tcp_client_socket.h"

#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"

namespace net {

//-----------------------------------------------------------------------------

static int MapWinsockError(DWORD err) {
  // There are numerous Winsock error codes, but these are the ones we thus far
  // find interesting.
  switch (err) {
    case WSAENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case WSAETIMEDOUT:
      return ERR_TIMED_OUT;
    case WSAECONNRESET:
    case WSAENETRESET:
      return ERR_CONNECTION_RESET;
    case WSAECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case WSAECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case WSAEDISCON:
      return ERR_CONNECTION_CLOSED;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case WSAEADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case ERROR_SUCCESS:
      return OK;
    default:
      return ERR_FAILED;
  }
}

//-----------------------------------------------------------------------------

TCPClientSocket::TCPClientSocket(const AddressList& addresses)
    : socket_(INVALID_SOCKET),
      addresses_(addresses),
      current_ai_(addresses_.head()),
      wait_state_(NOT_WAITING) {
  memset(&overlapped_, 0, sizeof(overlapped_));
  EnsureWinsockInit();
}

TCPClientSocket::~TCPClientSocket() {
  Disconnect();
}

int TCPClientSocket::Connect(CompletionCallback* callback) {
  // If already connected, then just return OK.
  if (socket_ != INVALID_SOCKET)
    return OK;

  const struct addrinfo* ai = current_ai_;
  DCHECK(ai);

  int rv = CreateSocket(ai);
  if (rv != OK)
    return rv;

  if (!connect(socket_, ai->ai_addr, static_cast<int>(ai->ai_addrlen))) {
    // Connected without waiting!
    return OK;
  }

  DWORD err = WSAGetLastError();
  if (err != WSAEWOULDBLOCK) {
    LOG(ERROR) << "connect failed: " << err;
    return MapWinsockError(err);
  }

  overlapped_.hEvent = WSACreateEvent();
  WSAEventSelect(socket_, overlapped_.hEvent, FD_CONNECT);

  watcher_.StartWatching(overlapped_.hEvent, this);
  wait_state_ = WAITING_CONNECT;
  callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPClientSocket::ReconnectIgnoringLastError(CompletionCallback* callback) {
  // No ignorable errors!
  return ERR_FAILED;
}

void TCPClientSocket::Disconnect() {
  if (socket_ == INVALID_SOCKET)
    return;

  // Make sure the message loop is not watching this object anymore.
  watcher_.StopWatching();

  // This cancels any pending IO.
  closesocket(socket_);
  socket_ = INVALID_SOCKET;

  WSACloseEvent(overlapped_.hEvent);
  overlapped_.hEvent = NULL;

  // Reset for next time.
  current_ai_ = addresses_.head();
}

bool TCPClientSocket::IsConnected() const {
  if (socket_ == INVALID_SOCKET || wait_state_ == WAITING_CONNECT)
    return false;

  // Check if connection is alive.
  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv == 0)
    return false;
  if (rv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    return false;

  return true;
}

int TCPClientSocket::Read(char* buf, int buf_len, CompletionCallback* callback) {
  DCHECK(socket_ != INVALID_SOCKET);
  DCHECK(wait_state_ == NOT_WAITING);
  DCHECK(!callback_);

  buffer_.len = buf_len;
  buffer_.buf = buf;

  DWORD num, flags = 0;
  int rv = WSARecv(socket_, &buffer_, 1, &num, &flags, &overlapped_, NULL);
  if (rv == 0)
    return static_cast<int>(num);
  if (rv == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING) {
    watcher_.StartWatching(overlapped_.hEvent, this);
    wait_state_ = WAITING_READ;
    callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(WSAGetLastError());
}

int TCPClientSocket::Write(const char* buf, int buf_len, CompletionCallback* callback) {
  DCHECK(socket_ != INVALID_SOCKET);
  DCHECK(wait_state_ == NOT_WAITING);
  DCHECK(!callback_);

  buffer_.len = buf_len;
  buffer_.buf = const_cast<char*>(buf);

  DWORD num;
  int rv = WSASend(socket_, &buffer_, 1, &num, 0, &overlapped_, NULL);
  if (rv == 0)
    return static_cast<int>(num);
  if (rv == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING) {
    watcher_.StartWatching(overlapped_.hEvent, this);
    wait_state_ = WAITING_WRITE;
    callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(WSAGetLastError());
}

int TCPClientSocket::CreateSocket(const struct addrinfo* ai) {
  socket_ = WSASocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol, NULL, 0,
                      WSA_FLAG_OVERLAPPED);
  if (socket_ == INVALID_SOCKET) {
    LOG(ERROR) << "WSASocket failed: " << WSAGetLastError();
    return ERR_FAILED;
  }

  // Configure non-blocking mode.
  u_long non_blocking_mode = 1;
  if (ioctlsocket(socket_, FIONBIO, &non_blocking_mode)) {
    LOG(ERROR) << "ioctlsocket failed: " << WSAGetLastError();
    return ERR_FAILED;
  }

  return OK;
}

void TCPClientSocket::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(callback_);

  // since Run may result in Read being called, clear callback_ up front.
  CompletionCallback* c = callback_;
  callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocket::DidCompleteConnect() {
  int result;

  wait_state_ = NOT_WAITING;

  WSANETWORKEVENTS events;
  WSAEnumNetworkEvents(socket_, overlapped_.hEvent, &events);
  if (events.lNetworkEvents & FD_CONNECT) {
    wait_state_ = NOT_WAITING;
    DWORD error_code = static_cast<DWORD>(events.iErrorCode[FD_CONNECT_BIT]);
    if (current_ai_->ai_next && (
        error_code == WSAEADDRNOTAVAIL ||
        error_code == WSAEAFNOSUPPORT ||
        error_code == WSAECONNREFUSED ||
        error_code == WSAENETUNREACH ||
        error_code == WSAEHOSTUNREACH ||
        error_code == WSAETIMEDOUT)) {
      // Try using the next address.
      const struct addrinfo* next = current_ai_->ai_next;
      Disconnect();
      current_ai_ = next;
      result = Connect(callback_);
    } else {
      result = MapWinsockError(error_code);
    }
  } else {
    NOTREACHED();
    result = ERR_FAILED;
  }

  if (result != ERR_IO_PENDING)
    DoCallback(result);
}

void TCPClientSocket::DidCompleteIO() {
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(
      socket_, &overlapped_, &num_bytes, FALSE, &flags);
  wait_state_ = NOT_WAITING;
  DoCallback(ok ? num_bytes : MapWinsockError(WSAGetLastError()));
}

void TCPClientSocket::OnObjectSignaled(HANDLE object) {
  DCHECK(object == overlapped_.hEvent);

  switch (wait_state_) {
    case WAITING_CONNECT:
      DidCompleteConnect();
      break;
    case WAITING_READ:
    case WAITING_WRITE:
      DidCompleteIO();
      break;
  }
}

}  // namespace net
