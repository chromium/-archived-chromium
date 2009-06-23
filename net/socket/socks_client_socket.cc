// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_client_socket.h"

#include "base/basictypes.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif
#include "base/compiler_specific.h"
#include "base/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"

namespace net {

// Every SOCKS server requests a user-id from the client. It is optional
// and we send an empty string.
static const char kEmptyUserId[] = "";

// The SOCKS4a implementation suggests to use an invalid IP in case the DNS
// resolution at client fails.
static const uint8 kInvalidIp[] = { 0, 0, 0, 127 };

// For SOCKS4, the client sends 8 bytes  plus the size of the user-id.
// For SOCKS4A, this increases to accomodate the unresolved hostname.
static const int kWriteHeaderSize = 8;

// For SOCKS4 and SOCKS4a, the server sends 8 bytes for acknowledgement.
static const int kReadHeaderSize = 8;

// Server Response codes for SOCKS.
static const uint8 kServerResponseOk  = 0x5A;
static const uint8 kServerResponseRejected = 0x5B;
static const uint8 kServerResponseNotReachable = 0x5C;
static const uint8 kServerResponseMismatchedUserId = 0x5D;

static const uint8 kSOCKSVersion4 = 0x04;
static const uint8 kSOCKSStreamRequest = 0x01;

// A struct holding the essential details of the SOCKS4/4a Server Request.
// The port in the header is stored in network byte order.
struct SOCKS4ServerRequest {
  uint8 version;
  uint8 command;
  uint16 nw_port;
  uint8 ip[4];
};
COMPILE_ASSERT(sizeof(SOCKS4ServerRequest) == kWriteHeaderSize,
               socks4_server_request_struct_wrong_size);

// A struct holding details of the SOCKS4/4a Server Response.
struct SOCKS4ServerResponse {
  uint8 reserved_null;
  uint8 code;
  uint16 port;
  uint8 ip[4];
};
COMPILE_ASSERT(sizeof(SOCKS4ServerResponse) == kReadHeaderSize,
               socks4_server_response_struct_wrong_size);

SOCKSClientSocket::SOCKSClientSocket(ClientSocket* transport_socket,
                                     const HostResolver::RequestInfo& req_info,
                                     HostResolver* host_resolver)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &SOCKSClientSocket::OnIOComplete)),
      transport_(transport_socket),
      next_state_(STATE_NONE),
      socks_version_(kSOCKS4Unresolved),
      user_callback_(NULL),
      handshake_buf_len_(0),
      buffer_(NULL),
      buffer_len_(0),
      completed_handshake_(false),
      bytes_sent_(0),
      bytes_received_(0),
      resolver_(host_resolver),
      host_request_info_(req_info) {
}

SOCKSClientSocket::~SOCKSClientSocket() {
  Disconnect();
}

int SOCKSClientSocket::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(transport_->IsConnected());
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!user_callback_);

  // If already connected, then just return OK.
  if (completed_handshake_)
    return OK;

  next_state_ = STATE_RESOLVE_HOST;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void SOCKSClientSocket::Disconnect() {
  completed_handshake_ = false;
  transport_->Disconnect();
}

bool SOCKSClientSocket::IsConnected() const {
  return completed_handshake_ && transport_->IsConnected();
}

bool SOCKSClientSocket::IsConnectedAndIdle() const {
  return completed_handshake_ && transport_->IsConnectedAndIdle();
}

// Read is called by the transport layer above to read. This can only be done
// if the SOCKS handshake is complete.
int SOCKSClientSocket::Read(IOBuffer* buf, int buf_len,
                            CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!user_callback_);

  return transport_->Read(buf, buf_len, callback);
}

// Write is called by the transport layer. This can only be done if the
// SOCKS handshake is complete.
int SOCKSClientSocket::Write(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!user_callback_);

  return transport_->Write(buf, buf_len, callback);
}

void SOCKSClientSocket::DoCallback(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(user_callback_);

  // Since Run() may result in Read being called,
  // clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  DLOG(INFO) << "Finished setting up SOCKS handshake";
  c->Run(result);
}

void SOCKSClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int SOCKSClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        DCHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_HANDSHAKE_WRITE:
        DCHECK_EQ(OK, rv);
        rv = DoHandshakeWrite();
        break;
      case STATE_HANDSHAKE_WRITE_COMPLETE:
        rv = DoHandshakeWriteComplete(rv);
        break;
      case STATE_HANDSHAKE_READ:
        DCHECK_EQ(OK, rv);
        rv = DoHandshakeRead();
        break;
      case STATE_HANDSHAKE_READ_COMPLETE:
        rv = DoHandshakeReadComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int SOCKSClientSocket::DoResolveHost() {
  DCHECK_EQ(kSOCKS4Unresolved, socks_version_);

  next_state_ = STATE_RESOLVE_HOST_COMPLETE;
  return resolver_.Resolve(host_request_info_, &addresses_, &io_callback_);
}

int SOCKSClientSocket::DoResolveHostComplete(int result) {
  DCHECK_EQ(kSOCKS4Unresolved, socks_version_);

  bool ok = (result == OK);
  next_state_ = STATE_HANDSHAKE_WRITE;
  if (ok) {
    DCHECK(addresses_.head());

    // If the host is resolved to an IPv6 address, we revert to SOCKS4a
    // since IPv6 is unsupported by SOCKS4/4a protocol.
    struct sockaddr *host_info = addresses_.head()->ai_addr;
    if (host_info->sa_family == AF_INET) {
      DLOG(INFO) << "Resolved host. Using SOCKS4 to communicate";
      socks_version_ = kSOCKS4;
    } else {
      DLOG(INFO) << "Resolved host but to IPv6. Using SOCKS4a to communicate";
      socks_version_ = kSOCKS4a;
    }
  } else {
    DLOG(INFO) << "Could not resolve host. Using SOCKS4a to communicate";
    socks_version_ = kSOCKS4a;
  }

  // Even if DNS resolution fails, we send OK since the server
  // resolves the domain.
  return OK;
}

// Builds the buffer that is to be sent to the server.
// We check whether the SOCKS proxy is 4 or 4A.
// In case it is 4A, the record size increases by size of the hostname.
void SOCKSClientSocket::BuildHandshakeWriteBuffer() {
  DCHECK_NE(kSOCKS4Unresolved, socks_version_);

  int record_size = kWriteHeaderSize + arraysize(kEmptyUserId);
  if (socks_version_ == kSOCKS4a) {
    record_size += host_request_info_.hostname().size() + 1;
  }

  buffer_len_ = record_size;
  buffer_.reset(new char[buffer_len_]);

  SOCKS4ServerRequest* request =
      reinterpret_cast<SOCKS4ServerRequest*>(buffer_.get());

  request->version = kSOCKSVersion4;
  request->command = kSOCKSStreamRequest;
  request->nw_port = htons(host_request_info_.port());

  if (socks_version_ == kSOCKS4) {
    const struct addrinfo* ai = addresses_.head();
    DCHECK(ai);
    // If the sockaddr is IPv6, we have already marked the version to socks4a
    // and so this step does not get hit.
    struct sockaddr_in *ipv4_host =
        reinterpret_cast<struct sockaddr_in*>(ai->ai_addr);
    memcpy(&request->ip, &(ipv4_host->sin_addr), sizeof(ipv4_host->sin_addr));

    DLOG(INFO) << "Resolved Host is : " << NetAddressToString(ai);
  } else if (socks_version_ == kSOCKS4a) {
    // invalid IP of the form 0.0.0.127
    memcpy(&request->ip, kInvalidIp, arraysize(kInvalidIp));
  } else {
    NOTREACHED();
  }

  memcpy(&buffer_[kWriteHeaderSize], kEmptyUserId, arraysize(kEmptyUserId));

  if (socks_version_ == kSOCKS4a) {
    memcpy(&buffer_[kWriteHeaderSize + arraysize(kEmptyUserId)],
           host_request_info_.hostname().c_str(),
           host_request_info_.hostname().size() + 1);
  }
}

// Writes the SOCKS handshake data to the underlying socket connection.
int SOCKSClientSocket::DoHandshakeWrite() {
  next_state_ = STATE_HANDSHAKE_WRITE_COMPLETE;

  if (!buffer_.get()) {
    BuildHandshakeWriteBuffer();
    bytes_sent_ = 0;
  }

  handshake_buf_len_ = buffer_len_ - bytes_sent_;
  DCHECK_GT(handshake_buf_len_, 0);
  handshake_buf_ = new IOBuffer(handshake_buf_len_);
  memcpy(handshake_buf_.get()->data(), &buffer_[bytes_sent_],
         handshake_buf_len_);
  return transport_->Write(handshake_buf_, handshake_buf_len_, &io_callback_);
}

int SOCKSClientSocket::DoHandshakeWriteComplete(int result) {
  DCHECK_NE(kSOCKS4Unresolved, socks_version_);

  if (result < 0)
    return result;

  bytes_sent_ += result;
  if (bytes_sent_ == buffer_len_) {
    next_state_ = STATE_HANDSHAKE_READ;
    buffer_.reset(NULL);
  } else if (bytes_sent_ < buffer_len_) {
    next_state_ = STATE_HANDSHAKE_WRITE;
  } else {
    return ERR_UNEXPECTED;
  }

  return OK;
}

int SOCKSClientSocket::DoHandshakeRead() {
  DCHECK_NE(kSOCKS4Unresolved, socks_version_);

  next_state_ = STATE_HANDSHAKE_READ_COMPLETE;

  if (!buffer_.get()) {
    buffer_.reset(new char[kReadHeaderSize]);
    buffer_len_ = kReadHeaderSize;
    bytes_received_ = 0;
  }

  handshake_buf_len_ = buffer_len_ - bytes_received_;
  handshake_buf_ = new IOBuffer(handshake_buf_len_);
  return transport_->Read(handshake_buf_, handshake_buf_len_, &io_callback_);
}

int SOCKSClientSocket::DoHandshakeReadComplete(int result) {
  DCHECK_NE(kSOCKS4Unresolved, socks_version_);

  if (result < 0)
    return result;
  if (bytes_received_ + result > buffer_len_)
    return ERR_INVALID_RESPONSE;

  memcpy(buffer_.get() + bytes_received_, handshake_buf_->data(), result);
  bytes_received_ += result;
  if (bytes_received_ < buffer_len_) {
    next_state_ = STATE_HANDSHAKE_READ;
    return OK;
  }

  SOCKS4ServerResponse* response =
      reinterpret_cast<SOCKS4ServerResponse*>(buffer_.get());

  if (response->reserved_null != 0x00) {
    LOG(ERROR) << "Unknown response from SOCKS server.";
    return ERR_INVALID_RESPONSE;
  }

  // TODO(arindam): Add SOCKS specific failure codes in net_error_list.h
  switch (response->code) {
    case kServerResponseOk:
      completed_handshake_ = true;
      return OK;
    case kServerResponseRejected:
      LOG(ERROR) << "SOCKS request rejected or failed";
      return ERR_FAILED;
    case kServerResponseNotReachable:
      LOG(ERROR) << "SOCKS request failed because client is not running "
                 << "identd (or not reachable from the server)";
      return ERR_NAME_NOT_RESOLVED;
    case kServerResponseMismatchedUserId:
      LOG(ERROR) << "SOCKS request failed because client's identd could "
                 << "not confirm the user ID string in the request";
      return ERR_FAILED;
    default:
      LOG(ERROR) << "SOCKS server sent unknown response";
      return ERR_INVALID_RESPONSE;
  }

  // Note: we ignore the last 6 bytes as specified by the SOCKS protocol
}

#if defined(OS_LINUX)
// Identical to posix system call getpeername().
// Needed by ssl_client_socket_nss.
int SOCKSClientSocket::GetPeerName(struct sockaddr *name, socklen_t *namelen) {
  // Default implementation just permits some unit tests to link.
  NOTREACHED();
  return ERR_UNEXPECTED;
}
#endif

}  // namespace net

