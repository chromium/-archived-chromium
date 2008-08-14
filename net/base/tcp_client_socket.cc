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
    case WSAENETRESET:  // Related to keep-alive
      return ERR_CONNECTION_RESET;
    case WSAECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case WSAECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case WSAEDISCON:
      // Returned by WSARecv or WSARecvFrom for message-oriented sockets (where
      // a return value of zero means a zero-byte message) to indicate graceful
      // connection shutdown.  We should not ever see this error code for TCP
      // sockets, which are byte stream oriented.
      NOTREACHED();
      return ERR_CONNECTION_CLOSED;
    case WSAEHOSTUNREACH:
    case WSAENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case WSAEADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case WSA_IO_INCOMPLETE:
      return ERR_UNEXPECTED;
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

  overlapped_.hEvent = WSACreateEvent();
  // WSAEventSelect sets the socket to non-blocking mode as a side effect.
  // Our connect() and recv() calls require that the socket be non-blocking.
  WSAEventSelect(socket_, overlapped_.hEvent, FD_CONNECT);

  if (!connect(socket_, ai->ai_addr, static_cast<int>(ai->ai_addrlen))) {
    // Connected without waiting!
    return OK;
  }

  DWORD err = WSAGetLastError();
  if (err != WSAEWOULDBLOCK) {
    LOG(ERROR) << "connect failed: " << err;
    return MapWinsockError(err);
  }

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

  // In most socket implementations, closing a socket results in a graceful
  // connection shutdown, but in Winsock we have to call shutdown explicitly.
  // See the MSDN page "Graceful Shutdown, Linger Options, and Socket Closure"
  // at http://msdn.microsoft.com/en-us/library/ms738547.aspx
  shutdown(socket_, SD_SEND);

  // This cancels any pending IO.
  closesocket(socket_);
  socket_ = INVALID_SOCKET;

  WSACloseEvent(overlapped_.hEvent);
  memset(&overlapped_, 0, sizeof(overlapped_));

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

int TCPClientSocket::Read(char* buf,
                          int buf_len,
                          CompletionCallback* callback) {
  DCHECK(socket_ != INVALID_SOCKET);
  DCHECK(wait_state_ == NOT_WAITING);
  DCHECK(!callback_);

  buffer_.len = buf_len;
  buffer_.buf = buf;

  // TODO(wtc): Remove the CHECKs after enough testing.
  CHECK(WaitForSingleObject(overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num, flags = 0;
  int rv = WSARecv(socket_, &buffer_, 1, &num, &flags, &overlapped_, NULL);
  if (rv == 0) {
    CHECK(WaitForSingleObject(overlapped_.hEvent, 0) == WAIT_OBJECT_0);
    BOOL ok = WSAResetEvent(overlapped_.hEvent);
    CHECK(ok);
    return static_cast<int>(num);
  }
  int err = WSAGetLastError();
  if (err == WSA_IO_PENDING) {
    watcher_.StartWatching(overlapped_.hEvent, this);
    wait_state_ = WAITING_READ;
    callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(err);
}

int TCPClientSocket::Write(const char* buf,
                           int buf_len,
                           CompletionCallback* callback) {
  DCHECK(socket_ != INVALID_SOCKET);
  DCHECK(wait_state_ == NOT_WAITING);
  DCHECK(!callback_);

  buffer_.len = buf_len;
  buffer_.buf = const_cast<char*>(buf);

  // TODO(wtc): Remove the CHECKs after enough testing.
  CHECK(WaitForSingleObject(overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num;
  int rv = WSASend(socket_, &buffer_, 1, &num, 0, &overlapped_, NULL);
  if (rv == 0) {
    CHECK(WaitForSingleObject(overlapped_.hEvent, 0) == WAIT_OBJECT_0);
    BOOL ok = WSAResetEvent(overlapped_.hEvent);
    CHECK(ok);
    return static_cast<int>(num);
  }
  int err = WSAGetLastError();
  if (err == WSA_IO_PENDING) {
    watcher_.StartWatching(overlapped_.hEvent, this);
    wait_state_ = WAITING_WRITE;
    callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(err);
}

int TCPClientSocket::CreateSocket(const struct addrinfo* ai) {
  socket_ = WSASocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol, NULL, 0,
                      WSA_FLAG_OVERLAPPED);
  if (socket_ == INVALID_SOCKET) {
    DWORD err = WSAGetLastError();
    LOG(ERROR) << "WSASocket failed: " << err;
    return MapWinsockError(err);
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
  int rv = WSAEnumNetworkEvents(socket_, overlapped_.hEvent, &events);
  if (rv == SOCKET_ERROR) {
    NOTREACHED();
    result = MapWinsockError(WSAGetLastError());
  } else if (events.lNetworkEvents & FD_CONNECT) {
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
  WSAResetEvent(overlapped_.hEvent);
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
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace net
