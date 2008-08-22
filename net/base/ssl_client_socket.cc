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

#include "net/base/ssl_client_socket.h"

#include <schnlsp.h>

#include "base/singleton.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"

#pragma comment(lib, "secur32.lib")

namespace net {

//-----------------------------------------------------------------------------

// Size of recv_buffer_
//
// Ciphertext is decrypted one SSL record at a time, so recv_buffer_ needs to
// have room for a full SSL record, with the header and trailer.  Here is the
// breakdown of the size:
//   5: SSL record header
//   16K: SSL record maximum size
//   64: >= SSL record trailer (16 or 20 have been observed)
static const int kRecvBufferSize = (5 + 16*1024 + 64);

SSLClientSocket::SSLClientSocket(ClientSocket* transport_socket,
                                 const std::string& hostname)
#pragma warning(suppress: 4355)
    : io_callback_(this, &SSLClientSocket::OnIOComplete),
      transport_(transport_socket),
      hostname_(hostname),
      user_callback_(NULL),
      user_buf_(NULL),
      user_buf_len_(0),
      next_state_(STATE_NONE),
      payload_send_buffer_len_(0),
      bytes_sent_(0),
      decrypted_ptr_(NULL),
      bytes_decrypted_(0),
      received_ptr_(NULL),
      bytes_received_(0),
      completed_handshake_(false),
      ignore_ok_result_(false) {
  memset(&stream_sizes_, 0, sizeof(stream_sizes_));
  memset(&send_buffer_, 0, sizeof(send_buffer_));
  memset(&creds_, 0, sizeof(creds_));
  memset(&ctxt_, 0, sizeof(ctxt_));
}

SSLClientSocket::~SSLClientSocket() {
  Disconnect();
}

int SSLClientSocket::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  next_state_ = STATE_CONNECT;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int SSLClientSocket::ReconnectIgnoringLastError(CompletionCallback* callback) {
  // TODO(darin): implement me!
  return ERR_FAILED;
}

void SSLClientSocket::Disconnect() {
  // TODO(wtc): Send SSL close_notify alert.
  completed_handshake_ = false;
  transport_->Disconnect();

  if (send_buffer_.pvBuffer) {
    FreeContextBuffer(send_buffer_.pvBuffer);
    memset(&send_buffer_, 0, sizeof(send_buffer_));
  }
  if (creds_.dwLower || creds_.dwUpper) {
    FreeCredentialsHandle(&creds_);
    memset(&creds_, 0, sizeof(creds_));
  }
  if (ctxt_.dwLower || ctxt_.dwUpper) {
    DeleteSecurityContext(&ctxt_);
    memset(&ctxt_, 0, sizeof(ctxt_));
  }
  // TODO(wtc): reset more members?
  bytes_decrypted_ = 0;
  bytes_received_ = 0;
}

bool SSLClientSocket::IsConnected() const {
  return completed_handshake_ && transport_->IsConnected();
}

int SSLClientSocket::Read(char* buf, int buf_len,
                          CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  // If we have surplus decrypted plaintext, satisfy the Read with it without
  // reading more ciphertext from the transport socket.
  if (bytes_decrypted_ != 0) {
    int len = std::min(buf_len, bytes_decrypted_);
    memcpy(buf, decrypted_ptr_, len);
    decrypted_ptr_ += len;
    bytes_decrypted_ -= len;
    if (bytes_decrypted_ == 0) {
      decrypted_ptr_ = NULL;
      if (bytes_received_ != 0) {
        memmove(recv_buffer_.get(), received_ptr_, bytes_received_);
        received_ptr_ = recv_buffer_.get();
      }
    }
    return len;
  }

  user_buf_ = buf;
  user_buf_len_ = buf_len;

  if (bytes_received_ == 0) {
    next_state_ = STATE_PAYLOAD_READ;
  } else {
    next_state_ = STATE_PAYLOAD_READ_COMPLETE;
    ignore_ok_result_ = true;  // OK doesn't mean EOF.
  }
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int SSLClientSocket::Write(const char* buf, int buf_len,
                           CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  user_buf_ = const_cast<char*>(buf);
  user_buf_len_ = buf_len;

  next_state_ = STATE_PAYLOAD_ENCRYPT;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void SSLClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  SECURITY_STATUS status;
  PCCERT_CONTEXT server_cert = NULL;
  status = QueryContextAttributes(&ctxt_,
                                  SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                                  &server_cert);
  if (status == SEC_E_OK) {
    DCHECK(server_cert);
    ssl_info->cert = X509Certificate::CreateFromHandle(server_cert);
  }
  SecPkgContext_ConnectionInfo connection_info;
  status = QueryContextAttributes(&ctxt_,
                                  SECPKG_ATTR_CONNECTION_INFO,
                                  &connection_info);
  if (status == SEC_E_OK) {
    // TODO(wtc): compute the overall security strength, taking into account
    // dwExchStrength and dwHashStrength.  dwExchStrength needs to be
    // normalized.
    ssl_info->security_bits = connection_info.dwCipherStrength;
  }
  ssl_info->cert_status = 0;
}

void SSLClientSocket::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void SSLClientSocket::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int SSLClientSocket::DoLoop(int last_io_result) {
  DCHECK(next_state_ != STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_CONNECT:
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      case STATE_HANDSHAKE_READ:
        rv = DoHandshakeRead();
        break;
      case STATE_HANDSHAKE_READ_COMPLETE:
        rv = DoHandshakeReadComplete(rv);
        break;
      case STATE_HANDSHAKE_WRITE:
        rv = DoHandshakeWrite();
        break;
      case STATE_HANDSHAKE_WRITE_COMPLETE:
        rv = DoHandshakeWriteComplete(rv);
        break;
      case STATE_PAYLOAD_READ:
        rv = DoPayloadRead();
        break;
      case STATE_PAYLOAD_READ_COMPLETE:
        rv = DoPayloadReadComplete(rv);
        break;
      case STATE_PAYLOAD_ENCRYPT:
        rv = DoPayloadEncrypt();
        break;
      case STATE_PAYLOAD_WRITE:
        rv = DoPayloadWrite();
        break;
      case STATE_PAYLOAD_WRITE_COMPLETE:
        rv = DoPayloadWriteComplete(rv);
        break;
      default:
        rv = ERR_FAILED;
        NOTREACHED() << "unexpected state";
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocket::DoConnect() {
  next_state_ = STATE_CONNECT_COMPLETE;
  return transport_->Connect(&io_callback_);
}

int SSLClientSocket::DoConnectComplete(int result) {
  if (result < 0)
    return result;

  memset(&ctxt_, 0, sizeof(ctxt_));
  memset(&creds_, 0, sizeof(creds_));

  SCHANNEL_CRED schannel_cred = {0};
  schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;

  // TODO(wtc): This should be configurable.  Hardcoded to do SSL 3.0 and
  // TLS 1.0 for now.  The default (0) means Schannel selects the protocol.
  // The global system registry settings take precedence over this value.
  schannel_cred.grbitEnabledProtocols = SP_PROT_SSL3TLS1;

  // The default session lifetime is 36000000 milliseconds (ten hours).  Set
  // schannel_cred.dwSessionLifespan to change the number of milliseconds that
  // Schannel keeps the session in its session cache.

  // We can set the key exchange algorithms (RSA or DH) in
  // schannel_cred.{cSupportedAlgs,palgSupportedAlgs}.

  // TODO(wtc): We may need to use SCH_CRED_IGNORE_NO_REVOCATION_CHECK and
  // SCH_CRED_IGNORE_REVOCATION_OFFLINE, but only after getting the
  // CRYPT_E_NO_REVOCATION_CHECK and CRYPT_E_REVOCATION_OFFLINE errors.
  //
  // Look into undocumented or poorly documented flags:
  //   SCH_CRED_RESTRICTED_ROOTS
  //   SCH_CRED_REVOCATION_CHECK_CACHE_ONLY
  //   SCH_CRED_CACHE_ONLY_URL_RETRIEVAL
  //   SCH_CRED_MEMORY_STORE_CERT
  //   SCH_CRED_CACHE_ONLY_URL_RETRIEVAL_ON_CREATE
  //
  // SCH_CRED_NO_SERVERNAME_CHECK can be useful during testing.
  schannel_cred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS |
                           SCH_CRED_NO_SERVERNAME_CHECK |  // Remove me!
                           SCH_CRED_AUTO_CRED_VALIDATION |
                           SCH_CRED_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  TimeStamp expiry;
  SECURITY_STATUS status;

  status = AcquireCredentialsHandle(
      NULL,  // Not used
      UNISP_NAME,  // Microsoft Unified Security Protocol Provider
      SECPKG_CRED_OUTBOUND,
      NULL,  // Not used
      &schannel_cred,
      NULL,  // Not used
      NULL,  // Not used
      &creds_,
      &expiry);  // Optional
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "AcquireCredentialsHandle failed: " << status;
    return ERR_FAILED;  // TODO(wtc): map SEC_E_xxx error codes.
  }

  SecBufferDesc buffer_desc;
  DWORD out_flags;
  DWORD flags = ISC_REQ_SEQUENCE_DETECT   |
                ISC_REQ_REPLAY_DETECT     |
                ISC_REQ_CONFIDENTIALITY   |
                ISC_RET_EXTENDED_ERROR    |
                ISC_REQ_ALLOCATE_MEMORY   |
                ISC_REQ_STREAM;

  send_buffer_.pvBuffer = NULL;
  send_buffer_.BufferType = SECBUFFER_TOKEN;
  send_buffer_.cbBuffer = 0;

  buffer_desc.cBuffers = 1;
  buffer_desc.pBuffers = &send_buffer_;
  buffer_desc.ulVersion = SECBUFFER_VERSION;

  status = InitializeSecurityContext(
      &creds_,
      NULL,  // NULL on the first call
      const_cast<wchar_t*>(ASCIIToWide(hostname_).c_str()),
      flags,
      0,  // Reserved
      SECURITY_NATIVE_DREP,  // TODO(wtc): MSDN says this should be set to 0.
      NULL,  // NULL on the first call
      0,  // Reserved
      &ctxt_,  // Receives the new context handle
      &buffer_desc,
      &out_flags,
      &expiry);
  if (status != SEC_I_CONTINUE_NEEDED) {
    DLOG(ERROR) << "InitializeSecurityContext failed: " << status;
    return ERR_FAILED;  // TODO(wtc): map SEC_E_xxx error codes.
  }

  next_state_ = STATE_HANDSHAKE_WRITE;
  return OK;
}

int SSLClientSocket::DoHandshakeRead() {
  next_state_ = STATE_HANDSHAKE_READ_COMPLETE;

  if (!recv_buffer_.get())
    recv_buffer_.reset(new char[kRecvBufferSize]);

  char* buf = recv_buffer_.get() + bytes_received_;
  int buf_len = kRecvBufferSize - bytes_received_;

  if (buf_len <= 0) {
    NOTREACHED() << "Receive buffer is too small!";
    return ERR_FAILED;
  }

  return transport_->Read(buf, buf_len, &io_callback_);
}

int SSLClientSocket::DoHandshakeReadComplete(int result) {
  if (result < 0)
    return result;
  if (result == 0 && !ignore_ok_result_)
    return ERR_FAILED;  // Incomplete response :(

  ignore_ok_result_ = false;

  bytes_received_ += result;

  // Process the contents of recv_buffer_.
  SECURITY_STATUS status;
  TimeStamp expiry;
  DWORD out_flags;

  DWORD flags = ISC_REQ_SEQUENCE_DETECT |
                ISC_REQ_REPLAY_DETECT |
                ISC_REQ_CONFIDENTIALITY |
                ISC_RET_EXTENDED_ERROR |
                ISC_REQ_ALLOCATE_MEMORY |
                ISC_REQ_STREAM;

  SecBufferDesc in_buffer_desc, out_buffer_desc;
  SecBuffer in_buffers[2];

  in_buffer_desc.cBuffers = 2;
  in_buffer_desc.pBuffers = in_buffers;
  in_buffer_desc.ulVersion = SECBUFFER_VERSION;

  in_buffers[0].pvBuffer = &recv_buffer_[0];
  in_buffers[0].cbBuffer = bytes_received_;
  in_buffers[0].BufferType = SECBUFFER_TOKEN;

  in_buffers[1].pvBuffer = NULL;
  in_buffers[1].cbBuffer = 0;
  in_buffers[1].BufferType = SECBUFFER_EMPTY;

  out_buffer_desc.cBuffers = 1;
  out_buffer_desc.pBuffers = &send_buffer_;
  out_buffer_desc.ulVersion = SECBUFFER_VERSION;

  send_buffer_.pvBuffer = NULL;
  send_buffer_.BufferType = SECBUFFER_TOKEN;
  send_buffer_.cbBuffer = 0;

  status = InitializeSecurityContext(
      &creds_,
      &ctxt_,
      NULL,
      flags,
      0,
      SECURITY_NATIVE_DREP,
      &in_buffer_desc,
      0,
      NULL,
      &out_buffer_desc,
      &out_flags,
      &expiry);

  if (status == SEC_E_INCOMPLETE_MESSAGE) {
    DCHECK(FAILED(status));
    DCHECK(send_buffer_.cbBuffer == 0 ||
           !(out_flags & ISC_RET_EXTENDED_ERROR));
    next_state_ = STATE_HANDSHAKE_READ;
    return OK;
  }

  if (send_buffer_.cbBuffer != 0 &&
      (status == SEC_E_OK ||
       status == SEC_I_CONTINUE_NEEDED ||
       FAILED(status) && (out_flags & ISC_RET_EXTENDED_ERROR))) {
    // TODO(wtc): if status is SEC_E_OK, we should finish the handshake
    // successfully after sending send_buffer_.
    // If FAILED(status) is true, we should terminate the connection after
    // sending send_buffer_.
    DCHECK(status == SEC_I_CONTINUE_NEEDED);  // We only handle this case
                                              // correctly.
    next_state_ = STATE_HANDSHAKE_WRITE;
    bytes_received_ = 0;
    return OK;
  }

  if (status == SEC_E_OK) {
    if (in_buffers[1].BufferType == SECBUFFER_EXTRA) {
      // TODO(darin) need to save this data for later.
      NOTREACHED() << "should not occur for HTTPS traffic";
      return ERR_FAILED;
    }
    bytes_received_ = 0;
    return DidCompleteHandshake();
  }

  if (FAILED(status))
    return ERR_FAILED;  // TODO(wtc): map error codes, in particular cert
                        // errors such as SEC_E_UNTRUSTED_ROOT.

  DCHECK(status == SEC_I_CONTINUE_NEEDED);
  if (in_buffers[1].BufferType == SECBUFFER_EXTRA) {
    memmove(&recv_buffer_[0],
            &recv_buffer_[0] + (bytes_received_ - in_buffers[1].cbBuffer),
            in_buffers[1].cbBuffer);
    bytes_received_ = in_buffers[1].cbBuffer;
    next_state_ = STATE_HANDSHAKE_READ_COMPLETE;
    ignore_ok_result_ = true;  // OK doesn't mean EOF.
    return OK;
  }

  bytes_received_ = 0;
  next_state_ = STATE_HANDSHAKE_READ;
  return OK;
}

int SSLClientSocket::DoHandshakeWrite() {
  next_state_ = STATE_HANDSHAKE_WRITE_COMPLETE;

  // We should have something to send.
  DCHECK(send_buffer_.pvBuffer);
  DCHECK(send_buffer_.cbBuffer > 0);

  const char* buf = static_cast<char*>(send_buffer_.pvBuffer) + bytes_sent_;
  int buf_len = send_buffer_.cbBuffer - bytes_sent_;

  return transport_->Write(buf, buf_len, &io_callback_);
}

int SSLClientSocket::DoHandshakeWriteComplete(int result) {
  if (result < 0)
    return result;

  DCHECK(result != 0);

  bytes_sent_ += result;
  DCHECK(bytes_sent_ <= static_cast<int>(send_buffer_.cbBuffer));

  if (bytes_sent_ >= static_cast<int>(send_buffer_.cbBuffer)) {
    bool overflow = (bytes_sent_ > static_cast<int>(send_buffer_.cbBuffer));
    SECURITY_STATUS status = FreeContextBuffer(send_buffer_.pvBuffer);
    DCHECK(status == SEC_E_OK);
    memset(&send_buffer_, 0, sizeof(send_buffer_));
    bytes_sent_ = 0;
    if (overflow)  // Bug!
      return ERR_UNEXPECTED;
    next_state_ = STATE_HANDSHAKE_READ;
  } else {
    // Send the remaining bytes.
    next_state_ = STATE_HANDSHAKE_WRITE;
  }

  return OK;
}

int SSLClientSocket::DoPayloadRead() {
  next_state_ = STATE_PAYLOAD_READ_COMPLETE;

  DCHECK(recv_buffer_.get());

  char* buf = recv_buffer_.get() + bytes_received_;
  int buf_len = kRecvBufferSize - bytes_received_;

  if (buf_len <= 0) {
    NOTREACHED() << "Receive buffer is too small!";
    return ERR_FAILED;
  }

  return transport_->Read(buf, buf_len, &io_callback_);
}

int SSLClientSocket::DoPayloadReadComplete(int result) {
  if (result < 0)
    return result;
  if (result == 0 && !ignore_ok_result_) {
    // TODO(wtc): Unless we have received the close_notify alert, we need to
    // return an error code indicating that the SSL connection ended
    // uncleanly, a potential truncation attack.
    if (bytes_received_ != 0)
      return ERR_FAILED;
    return OK;
  }

  ignore_ok_result_ = false;

  bytes_received_ += result;

  // Process the contents of recv_buffer_.
  SecBuffer buffers[4];
  buffers[0].pvBuffer = recv_buffer_.get();
  buffers[0].cbBuffer = bytes_received_;
  buffers[0].BufferType = SECBUFFER_DATA;

  buffers[1].BufferType = SECBUFFER_EMPTY;
  buffers[2].BufferType = SECBUFFER_EMPTY;
  buffers[3].BufferType = SECBUFFER_EMPTY;

  SecBufferDesc buffer_desc;
  buffer_desc.cBuffers = 4;
  buffer_desc.pBuffers = buffers;
  buffer_desc.ulVersion = SECBUFFER_VERSION;

  SECURITY_STATUS status;
  status = DecryptMessage(&ctxt_, &buffer_desc, 0, NULL);

  if (status == SEC_E_INCOMPLETE_MESSAGE) {
    next_state_ = STATE_PAYLOAD_READ;
    return OK;
  }

  if (status == SEC_I_CONTEXT_EXPIRED) {
    // Received the close_notify alert.
    bytes_received_ = 0;
    return OK;
  }

  if (status != SEC_E_OK && status != SEC_I_RENEGOTIATE) {
    DCHECK(status != SEC_E_MESSAGE_ALTERED);
    return ERR_FAILED;  // TODO(wtc): map error code
  }

  // The received ciphertext was decrypted in place in recv_buffer_.  Remember
  // the location and length of the decrypted plaintext and any unused
  // ciphertext.
  decrypted_ptr_ = NULL;
  bytes_decrypted_ = 0;
  received_ptr_ = NULL;
  bytes_received_ = 0;
  for (int i = 1; i < 4; i++) {
    if (!decrypted_ptr_ && buffers[i].BufferType == SECBUFFER_DATA) {
      decrypted_ptr_ = static_cast<char*>(buffers[i].pvBuffer);
      bytes_decrypted_ = buffers[i].cbBuffer;
    }
    if (!received_ptr_ && buffers[i].BufferType == SECBUFFER_EXTRA) {
      received_ptr_ = static_cast<char*>(buffers[i].pvBuffer);
      bytes_received_ = buffers[i].cbBuffer;
    }
  }

  int len = 0;
  if (bytes_decrypted_ != 0) {
    len = std::min(user_buf_len_, bytes_decrypted_);
    memcpy(user_buf_, decrypted_ptr_, len);
    decrypted_ptr_ += len;
    bytes_decrypted_ -= len;
  }
  if (bytes_decrypted_ == 0) {
    decrypted_ptr_ = NULL;
    if (bytes_received_ != 0) {
      memmove(recv_buffer_.get(), received_ptr_, bytes_received_);
      received_ptr_ = recv_buffer_.get();
    }
  }
  // TODO(wtc): need to handle SEC_I_RENEGOTIATE.
  DCHECK(status == SEC_E_OK);
  return len;
}

int SSLClientSocket::DoPayloadEncrypt() {
  DCHECK(user_buf_);
  DCHECK(user_buf_len_ > 0);

  ULONG message_len = std::min(
      stream_sizes_.cbMaximumMessage, static_cast<ULONG>(user_buf_len_));
  ULONG alloc_len =
      message_len + stream_sizes_.cbHeader + stream_sizes_.cbTrailer;
  user_buf_len_ = message_len;

  payload_send_buffer_.reset(new char[alloc_len]);
  memcpy(&payload_send_buffer_[stream_sizes_.cbHeader],
         user_buf_, message_len);

  SecBuffer buffers[4];
  buffers[0].pvBuffer = payload_send_buffer_.get();
  buffers[0].cbBuffer = stream_sizes_.cbHeader;
  buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

  buffers[1].pvBuffer = &payload_send_buffer_[stream_sizes_.cbHeader];
  buffers[1].cbBuffer = message_len;
  buffers[1].BufferType = SECBUFFER_DATA;

  buffers[2].pvBuffer = &payload_send_buffer_[stream_sizes_.cbHeader +
                                              message_len];
  buffers[2].cbBuffer = stream_sizes_.cbTrailer;
  buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

  buffers[3].BufferType = SECBUFFER_EMPTY;

  SecBufferDesc buffer_desc;
  buffer_desc.cBuffers = 4;
  buffer_desc.pBuffers = buffers;
  buffer_desc.ulVersion = SECBUFFER_VERSION;

  SECURITY_STATUS status = EncryptMessage(&ctxt_, 0, &buffer_desc, 0);

  if (FAILED(status))
    return ERR_FAILED;

  payload_send_buffer_len_ = buffers[0].cbBuffer +
                             buffers[1].cbBuffer +
                             buffers[2].cbBuffer;
  DCHECK(bytes_sent_ == 0);

  next_state_ = STATE_PAYLOAD_WRITE;
  return OK;
}

int SSLClientSocket::DoPayloadWrite() {
  next_state_ = STATE_PAYLOAD_WRITE_COMPLETE;

  // We should have something to send.
  DCHECK(payload_send_buffer_.get());
  DCHECK(payload_send_buffer_len_ > 0);

  const char* buf = payload_send_buffer_.get() + bytes_sent_;
  int buf_len = payload_send_buffer_len_ - bytes_sent_;

  return transport_->Write(buf, buf_len, &io_callback_);
}

int SSLClientSocket::DoPayloadWriteComplete(int result) {
  if (result < 0)
    return result;

  DCHECK(result != 0);

  bytes_sent_ += result;
  DCHECK(bytes_sent_ <= payload_send_buffer_len_);

  if (bytes_sent_ >= payload_send_buffer_len_) {
    bool overflow = (bytes_sent_ > payload_send_buffer_len_);
    payload_send_buffer_.reset();
    payload_send_buffer_len_ = 0;
    bytes_sent_ = 0;
    if (overflow)  // Bug!
      return ERR_UNEXPECTED;
    // Done
    return user_buf_len_;
  }

  // Send the remaining bytes.
  next_state_ = STATE_PAYLOAD_WRITE;
  return OK;
}

int SSLClientSocket::DidCompleteHandshake() {
  SECURITY_STATUS status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_STREAM_SIZES, &stream_sizes_);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "QueryContextAttributes failed: " << status;
    return ERR_FAILED;
  }

  completed_handshake_ = true;
  return OK;
}

}  // namespace net
