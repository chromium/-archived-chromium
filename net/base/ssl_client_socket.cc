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

namespace net {

//-----------------------------------------------------------------------------

class SChannelLib {
 public:
  SecurityFunctionTable funcs;

  SChannelLib() {
    memset(&funcs, 0, sizeof(funcs));
    lib_ = LoadLibrary(L"SCHANNEL.DLL");
    if (lib_) {
      INIT_SECURITY_INTERFACE init_security_interface =
          reinterpret_cast<INIT_SECURITY_INTERFACE>(
              GetProcAddress(lib_, "InitSecurityInterfaceW"));
      if (init_security_interface) {
        PSecurityFunctionTable funcs_ptr = init_security_interface();
        if (funcs_ptr)
          memcpy(&funcs, funcs_ptr, sizeof(funcs));
      }
    }
  }

  ~SChannelLib() {
    FreeLibrary(lib_);
  }

 private:
  HMODULE lib_;
};

static inline SecurityFunctionTable& SChannel() {
  return Singleton<SChannelLib>()->funcs;
}

//-----------------------------------------------------------------------------

static const int kRecvBufferSize = 0x10000;

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
      bytes_sent_(0),
      bytes_received_(0),
      completed_handshake_(false) {
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
  transport_->Disconnect();

  if (send_buffer_.pvBuffer) {
    SChannel().FreeContextBuffer(send_buffer_.pvBuffer);
    memset(&send_buffer_, 0, sizeof(send_buffer_));
  }
  if (creds_.dwLower || creds_.dwUpper) {
    SChannel().FreeCredentialsHandle(&creds_);
    memset(&creds_, 0, sizeof(creds_));
  }
  if (ctxt_.dwLower || ctxt_.dwUpper) {
    SChannel().DeleteSecurityContext(&ctxt_);
    memset(&ctxt_, 0, sizeof(ctxt_));
  }
}

bool SSLClientSocket::IsConnected() const {
  return completed_handshake_ && transport_->IsConnected();
}

int SSLClientSocket::Read(char* buf, int buf_len,
                          CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  user_buf_ = buf;
  user_buf_len_ = buf_len;

  next_state_ = STATE_PAYLOAD_READ;
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

  next_state_ = STATE_PAYLOAD_WRITE;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void SSLClientSocket::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // since Run may result in Read being called, clear callback_ up front.
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
  schannel_cred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS |
                            SCH_CRED_NO_SYSTEM_MAPPER |
                            SCH_CRED_REVOCATION_CHECK_CHAIN;
  TimeStamp expiry;
  SECURITY_STATUS status;

  status = SChannel().AcquireCredentialsHandle(
      NULL,
      UNISP_NAME,
      SECPKG_CRED_OUTBOUND,
      NULL,
      &schannel_cred,
      NULL,
      NULL,
      &creds_,
      &expiry);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "AcquireCredentialsHandle failed: " << status;
    return ERR_FAILED;
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

  status = SChannel().InitializeSecurityContext(
      &creds_,
      NULL,
      const_cast<wchar_t*>(ASCIIToWide(hostname_).c_str()),
      flags,
      0,
      SECURITY_NATIVE_DREP,
      NULL,
      0,
      &ctxt_,
      &buffer_desc,
      &out_flags,
      &expiry);
  if (status != SEC_I_CONTINUE_NEEDED) {
    DLOG(ERROR) << "InitializeSecurityContext failed: " << status;
    return ERR_FAILED;
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
  if (result == 0)
    return ERR_FAILED;  // Incomplete response :(

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

  status = SChannel().InitializeSecurityContext(
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
    next_state_ = STATE_HANDSHAKE_READ;
    return OK;
  }

  // OK, all of the received data was consumed.
  bytes_received_ = 0;

  if (send_buffer_.cbBuffer != 0 &&
      (status == SEC_E_OK ||
       status == SEC_I_CONTINUE_NEEDED ||
       FAILED(status) && (out_flags & ISC_RET_EXTENDED_ERROR))) {
    next_state_ = STATE_HANDSHAKE_WRITE;
    return OK;
  }

  if (status == SEC_E_OK) {
    if (in_buffers[1].BufferType == SECBUFFER_EXTRA) {
      // TODO(darin) need to save this data for later.
      NOTREACHED() << "should not occur for HTTPS traffic";
    }
    return DidCompleteHandshake();
  }

  if (FAILED(status))
    return ERR_FAILED;

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

  // TODO(darin): worry about overflow?
  bytes_sent_ += result;
  DCHECK(bytes_sent_ <= static_cast<int>(send_buffer_.cbBuffer));

  if (bytes_sent_ == static_cast<int>(send_buffer_.cbBuffer)) {
    SChannel().FreeContextBuffer(send_buffer_.pvBuffer);
    memset(&send_buffer_, 0, sizeof(send_buffer_));
    bytes_sent_ = 0;
    next_state_ = STATE_HANDSHAKE_READ;
  } else {
    // Send the remaining bytes.
    next_state_ = STATE_HANDSHAKE_WRITE;
  }

  return OK;
}

int SSLClientSocket::DoPayloadRead() {
  next_state_ = STATE_PAYLOAD_READ_COMPLETE;

  return ERR_FAILED;
}

int SSLClientSocket::DoPayloadReadComplete(int result) {
  return ERR_FAILED;
}

int SSLClientSocket::DoPayloadWrite() {
  DCHECK(user_buf_);
  DCHECK(user_buf_len_ > 0);

  next_state_ = STATE_PAYLOAD_WRITE_COMPLETE;

  size_t message_len = std::min(
      stream_sizes_.cbMaximumMessage, static_cast<ULONG>(user_buf_len_));
  size_t alloc_len =
      message_len + stream_sizes_.cbHeader + stream_sizes_.cbTrailer;

  /*
  SecBuffer buffers[4];
  buffers[0].

  SecBufferDesc buffer_desc;
  buffer_desc.cBuffers = 4;
  buffer_desc.pBuffers = //XXX
  buffer_desc.ulVersion = SECBUFFER_VERSION;

  SECURITY_STATUS status = SChannel().EncryptMessage(
      &ctxt_, 0, &buffer_desc, 0);
  */

  return ERR_FAILED;
}

int SSLClientSocket::DoPayloadWriteComplete(int result) {
  return ERR_FAILED;
}

int SSLClientSocket::DidCompleteHandshake() {
  SECURITY_STATUS status = SChannel().QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_STREAM_SIZES, &stream_sizes_);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "QueryContextAttributes failed: " << status;
    return ERR_FAILED;
  }

  // We expect not to have to worry about message padding.
  DCHECK(stream_sizes_.cbBlockSize == 1);

  completed_handshake_ = true;
  return OK;
}

}  // namespace net
