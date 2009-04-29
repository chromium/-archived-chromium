// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/tcp_client_socket_win.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory_debug.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"

namespace net {

namespace {

// Waits for the (manual-reset) event object to become signaled and resets
// it.  Called after a Winsock function succeeds synchronously
//
// Our testing shows that except in rare cases (when running inside QEMU),
// the event object is already signaled at this point, so we just call this
// method on the IO thread to avoid a context switch.
void WaitForAndResetEvent(WSAEVENT hEvent) {
  // TODO(wtc): Remove the CHECKs after enough testing.
  DWORD wait_rv = WaitForSingleObject(hEvent, INFINITE);
  CHECK(wait_rv == WAIT_OBJECT_0);
  BOOL ok = WSAResetEvent(hEvent);
  CHECK(ok);
}

//-----------------------------------------------------------------------------

int MapWinsockError(DWORD err) {
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
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TCPClientSocketWin::TCPClientSocketWin(const AddressList& addresses)
    : socket_(INVALID_SOCKET),
      addresses_(addresses),
      current_ai_(addresses_.head()),
      waiting_connect_(false),
      waiting_read_(false),
      waiting_write_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(reader_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(writer_(this)),
      read_callback_(NULL),
      write_callback_(NULL) {
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));
  EnsureWinsockInit();
}

TCPClientSocketWin::~TCPClientSocketWin() {
  Disconnect();
}

int TCPClientSocketWin::Connect(CompletionCallback* callback) {
  // If already connected, then just return OK.
  if (socket_ != INVALID_SOCKET)
    return OK;

  TRACE_EVENT_BEGIN("socket.connect", this, "");
  const struct addrinfo* ai = current_ai_;
  DCHECK(ai);

  int rv = CreateSocket(ai);
  if (rv != OK)
    return rv;

  // WSACreateEvent creates a manual-reset event object.
  read_overlapped_.hEvent = WSACreateEvent();
  // WSAEventSelect sets the socket to non-blocking mode as a side effect.
  // Our connect() and recv() calls require that the socket be non-blocking.
  WSAEventSelect(socket_, read_overlapped_.hEvent, FD_CONNECT);

  write_overlapped_.hEvent = WSACreateEvent();

  if (!connect(socket_, ai->ai_addr, static_cast<int>(ai->ai_addrlen))) {
    // Connected without waiting!
    WaitForAndResetEvent(read_overlapped_.hEvent);
    TRACE_EVENT_END("socket.connect", this, "");
    return OK;
  }

  DWORD err = WSAGetLastError();
  if (err != WSAEWOULDBLOCK) {
    LOG(ERROR) << "connect failed: " << err;
    return MapWinsockError(err);
  }

  read_watcher_.StartWatching(read_overlapped_.hEvent, &reader_);
  waiting_connect_ = true;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

void TCPClientSocketWin::Disconnect() {
  if (socket_ == INVALID_SOCKET)
    return;

  TRACE_EVENT_INSTANT("socket.disconnect", this, "");

  // Make sure the message loop is not watching this object anymore.
  read_watcher_.StopWatching();
  write_watcher_.StopWatching();

  // Note: don't use CancelIo to cancel pending IO because it doesn't work
  // when there is a Winsock layered service provider.

  // In most socket implementations, closing a socket results in a graceful
  // connection shutdown, but in Winsock we have to call shutdown explicitly.
  // See the MSDN page "Graceful Shutdown, Linger Options, and Socket Closure"
  // at http://msdn.microsoft.com/en-us/library/ms738547.aspx
  shutdown(socket_, SD_SEND);

  // This cancels any pending IO.
  closesocket(socket_);
  socket_ = INVALID_SOCKET;

  if (waiting_read_ || waiting_write_) {
    base::TimeTicks start = base::TimeTicks::Now();

    // Wait for pending IO to be aborted.
    if (waiting_read_)
      WaitForSingleObject(read_overlapped_.hEvent, INFINITE);
    if (waiting_write_)
      WaitForSingleObject(write_overlapped_.hEvent, INFINITE);

    // We want to see if we block the message loop for too long.
    UMA_HISTOGRAM_TIMES("AsyncIO.ClientSocketDisconnect",
                        base::TimeTicks::Now() - start);
  }

  WSACloseEvent(read_overlapped_.hEvent);
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  WSACloseEvent(write_overlapped_.hEvent);
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));

  // Reset for next time.
  current_ai_ = addresses_.head();

  waiting_read_ = false;
  waiting_write_ = false;
  waiting_connect_ = false;
}

bool TCPClientSocketWin::IsConnected() const {
  if (socket_ == INVALID_SOCKET || waiting_connect_)
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

bool TCPClientSocketWin::IsConnectedAndIdle() const {
  if (socket_ == INVALID_SOCKET || waiting_connect_)
    return false;

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv >= 0)
    return false;
  if (WSAGetLastError() != WSAEWOULDBLOCK)
    return false;

  return true;
}

int TCPClientSocketWin::Read(char* buf,
                             int buf_len,
                             CompletionCallback* callback) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!waiting_read_);
  DCHECK(!read_callback_);

  read_buffer_.len = buf_len;
  read_buffer_.buf = buf;

  TRACE_EVENT_BEGIN("socket.read", this, "");
  // TODO(wtc): Remove the CHECK after enough testing.
  CHECK(WaitForSingleObject(read_overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num, flags = 0;
  int rv = WSARecv(
      socket_, &read_buffer_, 1, &num, &flags, &read_overlapped_, NULL);
  if (rv == 0) {
    WaitForAndResetEvent(read_overlapped_.hEvent);
    TRACE_EVENT_END("socket.read", this, StringPrintf("%d bytes", num));

    // Because of how WSARecv fills memory when used asynchronously, Purify
    // isn't able to detect that it's been initialized, so it scans for 0xcd
    // in the buffer and reports UMRs (uninitialized memory reads) for those
    // individual bytes. We override that in PURIFY builds to avoid the false
    // error reports.
    // See bug 5297.
    base::MemoryDebug::MarkAsInitialized(read_buffer_.buf, num);
    return static_cast<int>(num);
  }
  int err = WSAGetLastError();
  if (err == WSA_IO_PENDING) {
    read_watcher_.StartWatching(read_overlapped_.hEvent, &reader_);
    waiting_read_ = true;
    read_callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(err);
}

int TCPClientSocketWin::Write(const char* buf,
                              int buf_len,
                              CompletionCallback* callback) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!waiting_write_);
  DCHECK(!write_callback_);
  DCHECK_GT(buf_len, 0);

  write_buffer_.len = buf_len;
  write_buffer_.buf = const_cast<char*>(buf);

  TRACE_EVENT_BEGIN("socket.write", this, "");
  // TODO(wtc): Remove the CHECK after enough testing.
  CHECK(WaitForSingleObject(write_overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num;
  int rv =
      WSASend(socket_, &write_buffer_, 1, &num, 0, &write_overlapped_, NULL);
  if (rv == 0) {
    WaitForAndResetEvent(write_overlapped_.hEvent);
    TRACE_EVENT_END("socket.write", this, StringPrintf("%d bytes", num));
    return static_cast<int>(num);
  }
  int err = WSAGetLastError();
  if (err == WSA_IO_PENDING) {
    write_watcher_.StartWatching(write_overlapped_.hEvent, &writer_);
    waiting_write_ = true;
    write_callback_ = callback;
    return ERR_IO_PENDING;
  }
  return MapWinsockError(err);
}

int TCPClientSocketWin::CreateSocket(const struct addrinfo* ai) {
  socket_ = WSASocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol, NULL, 0,
                      WSA_FLAG_OVERLAPPED);
  if (socket_ == INVALID_SOCKET) {
    DWORD err = WSAGetLastError();
    LOG(ERROR) << "WSASocket failed: " << err;
    return MapWinsockError(err);
  }

  // Increase the socket buffer sizes from the default sizes for WinXP.  In
  // performance testing, there is substantial benefit by increasing from 8KB
  // to 64KB.
  // See also:
  //    http://support.microsoft.com/kb/823764/EN-US
  // On Vista, if we manually set these sizes, Vista turns off its receive
  // window auto-tuning feature.
  //    http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
  // Since Vista's auto-tune is better than any static value we can could set,
  // only change these on pre-vista machines.
  int32 major_version, minor_version, fix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
    &fix_version);
  if (major_version < 6) {
    const int kSocketBufferSize = 64 * 1024;
    int rv = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
        reinterpret_cast<const char*>(&kSocketBufferSize),
        sizeof(kSocketBufferSize));
    DCHECK(!rv) << "Could not set socket send buffer size";
    rv = setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
        reinterpret_cast<const char*>(&kSocketBufferSize),
        sizeof(kSocketBufferSize));
    DCHECK(!rv) << "Could not set socket receive buffer size";
  }

  // Disable Nagle.
  // The Nagle implementation on windows is governed by RFC 896.  The idea
  // behind Nagle is to reduce small packets on the network.  When Nagle is
  // enabled, if a partial packet has been sent, the TCP stack will disallow
  // further *partial* packets until an ACK has been received from the other
  // side.  Good applications should always strive to send as much data as
  // possible and avoid partial-packet sends.  However, in most real world
  // applications, there are edge cases where this does not happen, and two
  // partil packets may be sent back to back.  For a browser, it is NEVER
  // a benefit to delay for an RTT before the second packet is sent.
  //
  // As a practical example in Chromium today, consider the case of a small
  // POST.  I have verified this:
  //     Client writes 649 bytes of header  (partial packet #1)
  //     Client writes 50 bytes of POST data (partial packet #2)
  // In the above example, with Nagle, a RTT delay is inserted between these
  // two sends due to nagle.  RTTs can easily be 100ms or more.  The best
  // fix is to make sure that for POSTing data, we write as much data as
  // possible and minimize partial packets.  We will fix that.  But disabling
  // Nagle also ensure we don't run into this delay in other edge cases.
  // See also:
  //    http://technet.microsoft.com/en-us/library/bb726981.aspx
  const BOOL kDisableNagle = TRUE;
  int rv = setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
      reinterpret_cast<const char*>(&kDisableNagle), sizeof(kDisableNagle));
  DCHECK(!rv) << "Could not disable nagle";

  return OK;
}

void TCPClientSocketWin::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(read_callback_);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback* c = read_callback_;
  read_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketWin::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketWin::DidCompleteConnect() {
  int result;

  TRACE_EVENT_END("socket.connect", this, "");
  waiting_connect_ = false;

  WSANETWORKEVENTS events;
  int rv = WSAEnumNetworkEvents(socket_, read_overlapped_.hEvent, &events);
  if (rv == SOCKET_ERROR) {
    NOTREACHED();
    result = MapWinsockError(WSAGetLastError());
  } else if (events.lNetworkEvents & FD_CONNECT) {
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
      result = Connect(read_callback_);
    } else {
      result = MapWinsockError(error_code);
    }
  } else {
    NOTREACHED();
    result = ERR_UNEXPECTED;
  }

  if (result != ERR_IO_PENDING)
    DoReadCallback(result);
}

void TCPClientSocketWin::ReadDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, tcp_socket_->read_overlapped_.hEvent);

  if (tcp_socket_->waiting_connect_) {
    tcp_socket_->DidCompleteConnect();
  } else {
    DWORD num_bytes, flags;
    BOOL ok = WSAGetOverlappedResult(
        tcp_socket_->socket_, &tcp_socket_->read_overlapped_, &num_bytes,
        FALSE, &flags);
    WSAResetEvent(object);
    TRACE_EVENT_END("socket.read", tcp_socket_,
                    StringPrintf("%d bytes", num_bytes));
    tcp_socket_->waiting_read_ = false;
    tcp_socket_->DoReadCallback(
        ok ? num_bytes : MapWinsockError(WSAGetLastError()));
  }
}

void TCPClientSocketWin::WriteDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, tcp_socket_->write_overlapped_.hEvent);

  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(
      tcp_socket_->socket_, &tcp_socket_->write_overlapped_, &num_bytes,
      FALSE, &flags);
  WSAResetEvent(object);
  TRACE_EVENT_END("socket.write", tcp_socket_,
                  StringPrintf("%d bytes", num_bytes));
  tcp_socket_->waiting_write_ = false;
  tcp_socket_->DoWriteCallback(
      ok ? num_bytes : MapWinsockError(WSAGetLastError()));
}

}  // namespace net
