// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_win.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory_debug.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"

namespace net {

namespace {

// If the (manual-reset) event object is signaled, resets it and returns true.
// Otherwise, does nothing and returns false.  Called after a Winsock function
// succeeds synchronously
//
// Our testing shows that except in rare cases (when running inside QEMU),
// the event object is already signaled at this point, so we call this method
// to avoid a context switch in common cases.  This is just a performance
// optimization.  The code still works if this function simply returns false.
bool ResetEventIfSignaled(WSAEVENT hEvent) {
  // TODO(wtc): Remove the CHECKs after enough testing.
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  if (wait_rv == WAIT_TIMEOUT)
    return false;  // The event object is not signaled.
  CHECK(wait_rv == WAIT_OBJECT_0);
  BOOL ok = WSAResetEvent(hEvent);
  CHECK(ok);
  return true;
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

// This class encapsulates all the state that has to be preserved as long as
// there is a network IO operation in progress. If the owner TCPClientSocketWin
// is destroyed while an operation is in progress, the Core is detached and it
// lives until the operation completes and the OS doesn't reference any resource
// declared on this class anymore.
class TCPClientSocketWin::Core : public base::RefCounted<Core> {
 public:
  explicit Core(TCPClientSocketWin* socket);
  ~Core();

  // Start watching for the end of a read or write operation.
  void WatchForRead();
  void WatchForWrite();

  // The TCPClientSocketWin is going away.
  void Detach() { socket_ = NULL; }

  // The separate OVERLAPPED variables for asynchronous operation.
  // |read_overlapped_| is used for both Connect() and Read().
  // |write_overlapped_| is only used for Write();
  OVERLAPPED read_overlapped_;
  OVERLAPPED write_overlapped_;

  // The buffers used in Read() and Write().
  WSABUF read_buffer_;
  WSABUF write_buffer_;
  scoped_refptr<IOBuffer> read_iobuffer_;
  scoped_refptr<IOBuffer> write_iobuffer_;

 private:
  class ReadDelegate : public base::ObjectWatcher::Delegate {
   public:
    explicit ReadDelegate(Core* core) : core_(core) {}
    virtual ~ReadDelegate() {}

    // base::ObjectWatcher::Delegate methods:
    virtual void OnObjectSignaled(HANDLE object);

   private:
    Core* const core_;
  };

  class WriteDelegate : public base::ObjectWatcher::Delegate {
   public:
    explicit WriteDelegate(Core* core) : core_(core) {}
    virtual ~WriteDelegate() {}

    // base::ObjectWatcher::Delegate methods:
    virtual void OnObjectSignaled(HANDLE object);

   private:
    Core* const core_;
  };

  // The socket that created this object.
  TCPClientSocketWin* socket_;

  // |reader_| handles the signals from |read_watcher_|.
  ReadDelegate reader_;
  // |writer_| handles the signals from |write_watcher_|.
  WriteDelegate writer_;

  // |read_watcher_| watches for events from Connect() and Read().
  base::ObjectWatcher read_watcher_;
  // |write_watcher_| watches for events from Write();
  base::ObjectWatcher write_watcher_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

TCPClientSocketWin::Core::Core(
    TCPClientSocketWin* socket)
    : socket_(socket),
      ALLOW_THIS_IN_INITIALIZER_LIST(reader_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(writer_(this)) {
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));
}

TCPClientSocketWin::Core::~Core() {
  // Make sure the message loop is not watching this object anymore.
  read_watcher_.StopWatching();
  write_watcher_.StopWatching();

  WSACloseEvent(read_overlapped_.hEvent);
  memset(&read_overlapped_, 0, sizeof(read_overlapped_));
  WSACloseEvent(write_overlapped_.hEvent);
  memset(&write_overlapped_, 0, sizeof(write_overlapped_));
}

void TCPClientSocketWin::Core::WatchForRead() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in ReadDelegate::OnObjectSignaled().
  AddRef();
  read_watcher_.StartWatching(read_overlapped_.hEvent, &reader_);
}

void TCPClientSocketWin::Core::WatchForWrite() {
  // We grab an extra reference because there is an IO operation in progress.
  // Balanced in WriteDelegate::OnObjectSignaled().
  AddRef();
  write_watcher_.StartWatching(write_overlapped_.hEvent, &writer_);
}

void TCPClientSocketWin::Core::ReadDelegate::OnObjectSignaled(
    HANDLE object) {
  DCHECK_EQ(object, core_->read_overlapped_.hEvent);
  if (core_->socket_) {
    if (core_->socket_->waiting_connect_) {
      core_->socket_->DidCompleteConnect();
    } else {
      core_->socket_->DidCompleteRead();
    }
  }

  core_->Release();
}

void TCPClientSocketWin::Core::WriteDelegate::OnObjectSignaled(
    HANDLE object) {
  DCHECK_EQ(object, core_->write_overlapped_.hEvent);
  if (core_->socket_)
    core_->socket_->DidCompleteWrite();

  core_->Release();
}

//-----------------------------------------------------------------------------

TCPClientSocketWin::TCPClientSocketWin(const AddressList& addresses)
    : socket_(INVALID_SOCKET),
      addresses_(addresses),
      current_ai_(addresses_.head()),
      waiting_connect_(false),
      waiting_read_(false),
      waiting_write_(false),
      read_callback_(NULL),
      write_callback_(NULL) {
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

  DCHECK(!core_);
  core_ = new Core(this);

  // WSACreateEvent creates a manual-reset event object.
  core_->read_overlapped_.hEvent = WSACreateEvent();
  // WSAEventSelect sets the socket to non-blocking mode as a side effect.
  // Our connect() and recv() calls require that the socket be non-blocking.
  WSAEventSelect(socket_, core_->read_overlapped_.hEvent, FD_CONNECT);

  core_->write_overlapped_.hEvent = WSACreateEvent();

  if (!connect(socket_, ai->ai_addr, static_cast<int>(ai->ai_addrlen))) {
    // Connected without waiting!
    //
    // The MSDN page for connect says:
    //   With a nonblocking socket, the connection attempt cannot be completed
    //   immediately. In this case, connect will return SOCKET_ERROR, and
    //   WSAGetLastError will return WSAEWOULDBLOCK.
    // which implies that for a nonblocking socket, connect never returns 0.
    // It's not documented whether the event object will be signaled or not
    // if connect does return 0.  So the code below is essentially dead code
    // and we don't know if it's correct.
    NOTREACHED();

    if (ResetEventIfSignaled(core_->read_overlapped_.hEvent)) {
      TRACE_EVENT_END("socket.connect", this, "");
      return OK;
    }
  } else {
    DWORD err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK) {
      LOG(ERROR) << "connect failed: " << err;
      return MapWinsockError(err);
    }
  }

  core_->WatchForRead();
  waiting_connect_ = true;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

void TCPClientSocketWin::Disconnect() {
  if (socket_ == INVALID_SOCKET)
    return;

  TRACE_EVENT_INSTANT("socket.disconnect", this, "");

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

  // Reset for next time.
  current_ai_ = addresses_.head();

  if (waiting_connect_) {
    // We closed the socket, so this notification will never come.
    // From MSDN' WSAEventSelect documentation:
    // "Closing a socket with closesocket also cancels the association and
    // selection of network events specified in WSAEventSelect for the socket".
    core_->Release();
  }

  waiting_read_ = false;
  waiting_write_ = false;
  waiting_connect_ = false;

  core_->Detach();
  core_ = NULL;
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

int TCPClientSocketWin::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionCallback* callback) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!waiting_read_);
  DCHECK(!read_callback_);
  DCHECK(!core_->read_iobuffer_);

  core_->read_buffer_.len = buf_len;
  core_->read_buffer_.buf = buf->data();

  TRACE_EVENT_BEGIN("socket.read", this, "");
  // TODO(wtc): Remove the CHECK after enough testing.
  CHECK(WaitForSingleObject(core_->read_overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num, flags = 0;
  int rv = WSARecv(socket_, &core_->read_buffer_, 1, &num, &flags,
                   &core_->read_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->read_overlapped_.hEvent)) {
      TRACE_EVENT_END("socket.read", this, StringPrintf("%d bytes", num));

      // Because of how WSARecv fills memory when used asynchronously, Purify
      // isn't able to detect that it's been initialized, so it scans for 0xcd
      // in the buffer and reports UMRs (uninitialized memory reads) for those
      // individual bytes. We override that in PURIFY builds to avoid the
      // false error reports.
      // See bug 5297.
      base::MemoryDebug::MarkAsInitialized(core_->read_buffer_.buf, num);
      return static_cast<int>(num);
    }
  } else {
    int err = WSAGetLastError();
    if (err != WSA_IO_PENDING)
      return MapWinsockError(err);
  }
  core_->WatchForRead();
  waiting_read_ = true;
  read_callback_ = callback;
  core_->read_iobuffer_ = buf;
  return ERR_IO_PENDING;
}

int TCPClientSocketWin::Write(IOBuffer* buf,
                              int buf_len,
                              CompletionCallback* callback) {
  DCHECK_NE(socket_, INVALID_SOCKET);
  DCHECK(!waiting_write_);
  DCHECK(!write_callback_);
  DCHECK_GT(buf_len, 0);
  DCHECK(!core_->write_iobuffer_);

  core_->write_buffer_.len = buf_len;
  core_->write_buffer_.buf = buf->data();

  TRACE_EVENT_BEGIN("socket.write", this, "");
  // TODO(wtc): Remove the CHECK after enough testing.
  CHECK(
      WaitForSingleObject(core_->write_overlapped_.hEvent, 0) == WAIT_TIMEOUT);
  DWORD num;
  int rv = WSASend(socket_, &core_->write_buffer_, 1, &num, 0,
                   &core_->write_overlapped_, NULL);
  if (rv == 0) {
    if (ResetEventIfSignaled(core_->write_overlapped_.hEvent)) {
      TRACE_EVENT_END("socket.write", this, StringPrintf("%d bytes", num));
      return static_cast<int>(num);
    }
  } else {
    int err = WSAGetLastError();
    if (err != WSA_IO_PENDING)
      return MapWinsockError(err);
  }
  core_->WatchForWrite();
  waiting_write_ = true;
  write_callback_ = callback;
  core_->write_iobuffer_ = buf;
  return ERR_IO_PENDING;
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
  DCHECK(waiting_connect_);
  int result;

  TRACE_EVENT_END("socket.connect", this, "");
  waiting_connect_ = false;

  WSANETWORKEVENTS events;
  int rv = WSAEnumNetworkEvents(socket_, core_->read_overlapped_.hEvent,
                                &events);
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

void TCPClientSocketWin::DidCompleteRead() {
  DCHECK(waiting_read_);
  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->read_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->read_overlapped_.hEvent);
  TRACE_EVENT_END("socket.read", this, StringPrintf("%d bytes", num_bytes));
  waiting_read_ = false;
  core_->read_iobuffer_ = NULL;
  DoReadCallback(ok ? num_bytes : MapWinsockError(WSAGetLastError()));
}

void TCPClientSocketWin::DidCompleteWrite() {
  DCHECK(waiting_write_);

  DWORD num_bytes, flags;
  BOOL ok = WSAGetOverlappedResult(socket_, &core_->write_overlapped_,
                                   &num_bytes, FALSE, &flags);
  WSAResetEvent(core_->write_overlapped_.hEvent);
  TRACE_EVENT_END("socket.write", this, StringPrintf("%d bytes", num_bytes));
  waiting_write_ = false;
  core_->write_iobuffer_ = NULL;
  DoWriteCallback(ok ? num_bytes : MapWinsockError(WSAGetLastError()));
}

}  // namespace net
