// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_client_socket_win.h"

#include <schnlsp.h>

#include "base/singleton.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/scoped_cert_chain_context.h"
#include "net/base/ssl_info.h"

#pragma comment(lib, "secur32.lib")

namespace net {

//-----------------------------------------------------------------------------

// TODO(wtc): See http://msdn.microsoft.com/en-us/library/aa377188(VS.85).aspx
// for the other error codes we may need to map.
static int MapSecurityError(SECURITY_STATUS err) {
  // There are numerous security error codes, but these are the ones we thus
  // far find interesting.
  switch (err) {
    case SEC_E_WRONG_PRINCIPAL:  // Schannel
    case CERT_E_CN_NO_MATCH:  // CryptoAPI
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_E_UNTRUSTED_ROOT:  // Schannel
    case CERT_E_UNTRUSTEDROOT:  // CryptoAPI
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_E_CERT_EXPIRED:  // Schannel
    case CERT_E_EXPIRED:  // CryptoAPI
      return ERR_CERT_DATE_INVALID;
    case CRYPT_E_NO_REVOCATION_CHECK:
      return ERR_CERT_NO_REVOCATION_MECHANISM;
    case CRYPT_E_REVOCATION_OFFLINE:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case CRYPT_E_REVOKED:  // Schannel and CryptoAPI
      return ERR_CERT_REVOKED;
    case SEC_E_CERT_UNKNOWN:
    case CERT_E_ROLE:
      return ERR_CERT_INVALID;
    // We received an unexpected_message or illegal_parameter alert message
    // from the server.
    case SEC_E_ILLEGAL_MESSAGE:
      return ERR_SSL_PROTOCOL_ERROR;
    case SEC_E_ALGORITHM_MISMATCH:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SEC_E_OK:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map a network error code to the equivalent certificate status flag.  If
// the error code is not a certificate error, it is mapped to 0.
static int MapNetErrorToCertStatus(int error) {
  switch (error) {
    case ERR_CERT_COMMON_NAME_INVALID:
      return CERT_STATUS_COMMON_NAME_INVALID;
    case ERR_CERT_DATE_INVALID:
      return CERT_STATUS_DATE_INVALID;
    case ERR_CERT_AUTHORITY_INVALID:
      return CERT_STATUS_AUTHORITY_INVALID;
    case ERR_CERT_NO_REVOCATION_MECHANISM:
      return CERT_STATUS_NO_REVOCATION_MECHANISM;
    case ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
    case ERR_CERT_REVOKED:
      return CERT_STATUS_REVOKED;
    case ERR_CERT_CONTAINS_ERRORS:
      NOTREACHED();
      // Falls through.
    case ERR_CERT_INVALID:
      return CERT_STATUS_INVALID;
    default:
      return 0;
  }
}

static int MapCertStatusToNetError(int cert_status) {
  // A certificate may have multiple errors.  We report the most
  // serious error.

  // Unrecoverable errors
  if (cert_status & CERT_STATUS_INVALID)
    return ERR_CERT_INVALID;
  if (cert_status & CERT_STATUS_REVOKED)
    return ERR_CERT_REVOKED;

  // Recoverable errors
  if (cert_status & CERT_STATUS_AUTHORITY_INVALID)
    return ERR_CERT_AUTHORITY_INVALID;
  if (cert_status & CERT_STATUS_COMMON_NAME_INVALID)
    return ERR_CERT_COMMON_NAME_INVALID;
  if (cert_status & CERT_STATUS_DATE_INVALID)
    return ERR_CERT_DATE_INVALID;

  // Unknown status.  Give it the benefit of the doubt.
  if (cert_status & CERT_STATUS_UNABLE_TO_CHECK_REVOCATION)
    return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
  if (cert_status & CERT_STATUS_NO_REVOCATION_MECHANISM)
    return ERR_CERT_NO_REVOCATION_MECHANISM;

  NOTREACHED();
  return ERR_UNEXPECTED;
}

// Map the errors in the chain_context->TrustStatus.dwErrorStatus returned by
// CertGetCertificateChain to our certificate status flags.
static int MapCertChainErrorStatusToCertStatus(DWORD error_status) {
  int cert_status = 0;

  // CERT_TRUST_IS_NOT_TIME_NESTED means a subject certificate's time validity
  // does not nest correctly within its issuer's time validity.
  const DWORD kDateInvalidErrors = CERT_TRUST_IS_NOT_TIME_VALID |
                                   CERT_TRUST_IS_NOT_TIME_NESTED |
                                   CERT_TRUST_CTL_IS_NOT_TIME_VALID;
  if (error_status & kDateInvalidErrors)
    cert_status |= CERT_STATUS_DATE_INVALID;

  const DWORD kAuthorityInvalidErrors = CERT_TRUST_IS_UNTRUSTED_ROOT |
                                        CERT_TRUST_IS_EXPLICIT_DISTRUST |
                                        CERT_TRUST_IS_PARTIAL_CHAIN;
  if (error_status & kAuthorityInvalidErrors)
    cert_status |= CERT_STATUS_AUTHORITY_INVALID;

  if ((error_status & CERT_TRUST_REVOCATION_STATUS_UNKNOWN) &&
      !(error_status & CERT_TRUST_IS_OFFLINE_REVOCATION))
    cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (error_status & CERT_TRUST_IS_OFFLINE_REVOCATION)
    cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  if (error_status & CERT_TRUST_IS_REVOKED)
    cert_status |= CERT_STATUS_REVOKED;

  const DWORD kWrongUsageErrors = CERT_TRUST_IS_NOT_VALID_FOR_USAGE |
                                  CERT_TRUST_CTL_IS_NOT_VALID_FOR_USAGE;
  if (error_status & kWrongUsageErrors) {
    // TODO(wtc): Handle these errors.
    // cert_status = |= CERT_STATUS_WRONG_USAGE;
  }

  // The rest of the errors.
  const DWORD kCertInvalidErrors =
      CERT_TRUST_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_IS_CYCLIC |
      CERT_TRUST_INVALID_EXTENSION |
      CERT_TRUST_INVALID_POLICY_CONSTRAINTS |
      CERT_TRUST_INVALID_BASIC_CONSTRAINTS |
      CERT_TRUST_INVALID_NAME_CONSTRAINTS |
      CERT_TRUST_CTL_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_HAS_NOT_SUPPORTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_DEFINED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_PERMITTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_EXCLUDED_NAME_CONSTRAINT |
      CERT_TRUST_NO_ISSUANCE_CHAIN_POLICY |
      CERT_TRUST_HAS_NOT_SUPPORTED_CRITICAL_EXT;
  if (error_status & kCertInvalidErrors)
    cert_status |= CERT_STATUS_INVALID;

  return cert_status;
}

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

SSLClientSocketWin::SSLClientSocketWin(ClientSocket* transport_socket,
                                       const std::string& hostname,
                                       const SSLConfig& ssl_config)
#pragma warning(suppress: 4355)
    : io_callback_(this, &SSLClientSocketWin::OnIOComplete),
      transport_(transport_socket),
      hostname_(hostname),
      ssl_config_(ssl_config),
      user_callback_(NULL),
      user_buf_(NULL),
      user_buf_len_(0),
      next_state_(STATE_NONE),
      server_cert_(NULL),
      server_cert_status_(0),
      payload_send_buffer_len_(0),
      bytes_sent_(0),
      decrypted_ptr_(NULL),
      bytes_decrypted_(0),
      received_ptr_(NULL),
      bytes_received_(0),
      completed_handshake_(false),
      complete_handshake_on_write_complete_(false),
      ignore_ok_result_(false),
      no_client_cert_(false) {
  memset(&stream_sizes_, 0, sizeof(stream_sizes_));
  memset(&send_buffer_, 0, sizeof(send_buffer_));
  memset(&creds_, 0, sizeof(creds_));
  memset(&ctxt_, 0, sizeof(ctxt_));
}

SSLClientSocketWin::~SSLClientSocketWin() {
  Disconnect();
}

void SSLClientSocketWin::GetSSLInfo(SSLInfo* ssl_info) {
  SECURITY_STATUS status = SEC_E_OK;
  if (server_cert_ == NULL) {
    status = QueryContextAttributes(&ctxt_,
                                    SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                                    &server_cert_);
  }
  if (status == SEC_E_OK) {
    DCHECK(server_cert_);
    PCCERT_CONTEXT dup_cert = CertDuplicateCertificateContext(server_cert_);
    ssl_info->cert = X509Certificate::CreateFromHandle(dup_cert);
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
  ssl_info->cert_status = server_cert_status_;
}

int SSLClientSocketWin::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  next_state_ = STATE_CONNECT;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int SSLClientSocketWin::ReconnectIgnoringLastError(
    CompletionCallback* callback) {
  // TODO(darin): implement me!
  return ERR_FAILED;
}

void SSLClientSocketWin::Disconnect() {
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
  if (server_cert_) {
    CertFreeCertificateContext(server_cert_);
    server_cert_ = NULL;
  }

  // TODO(wtc): reset more members?
  bytes_decrypted_ = 0;
  bytes_received_ = 0;
}

bool SSLClientSocketWin::IsConnected() const {
  // Ideally, we should also check if we have received the close_notify alert
  // message from the server, and return false in that case.  We're not doing
  // that, so this function may return a false positive.  Since the upper
  // layer (HttpNetworkTransaction) needs to handle a persistent connection
  // closed by the server when we send a request anyway, a false positive in
  // exchange for simpler code is a good trade-off.
  return completed_handshake_ && transport_->IsConnected();
}

int SSLClientSocketWin::Read(char* buf, int buf_len,
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

int SSLClientSocketWin::Write(const char* buf, int buf_len,
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

void SSLClientSocketWin::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void SSLClientSocketWin::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int SSLClientSocketWin::DoLoop(int last_io_result) {
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
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state";
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketWin::DoConnect() {
  next_state_ = STATE_CONNECT_COMPLETE;
  return transport_->Connect(&io_callback_);
}

int SSLClientSocketWin::DoConnectComplete(int result) {
  if (result < 0)
    return result;

  memset(&ctxt_, 0, sizeof(ctxt_));
  memset(&creds_, 0, sizeof(creds_));

  SCHANNEL_CRED schannel_cred = {0};
  schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;

  // The global system registry settings take precedence over the value of
  // schannel_cred.grbitEnabledProtocols.
  schannel_cred.grbitEnabledProtocols = 0;
  if (ssl_config_.ssl2_enabled)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_SSL2;
  if (ssl_config_.ssl3_enabled)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_SSL3;
  if (ssl_config_.tls1_enabled)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_TLS1;
  // The default (0) means Schannel selects the protocol, rather than no
  // protocols are selected.  So we have to fail here.
  if (schannel_cred.grbitEnabledProtocols == 0)
    return ERR_NO_SSL_VERSIONS_ENABLED;

  // The default session lifetime is 36000000 milliseconds (ten hours).  Set
  // schannel_cred.dwSessionLifespan to change the number of milliseconds that
  // Schannel keeps the session in its session cache.

  // We can set the key exchange algorithms (RSA or DH) in
  // schannel_cred.{cSupportedAlgs,palgSupportedAlgs}.

  // Although SCH_CRED_AUTO_CRED_VALIDATION is convenient, we have to use
  // SCH_CRED_MANUAL_CRED_VALIDATION for three reasons.
  // 1. SCH_CRED_AUTO_CRED_VALIDATION doesn't allow us to get the certificate
  //    context if the certificate validation fails.
  // 2. SCH_CRED_AUTO_CRED_VALIDATION returns only one error even if the
  //    certificate has multiple errors.
  // 3. SCH_CRED_AUTO_CRED_VALIDATION doesn't allow us to ignore untrusted CA
  //    and expired certificate errors.  There are only flags to ignore the
  //    name mismatch and unable-to-check-revocation errors.
  //
  // TODO(wtc): Look into undocumented or poorly documented flags:
  //   SCH_CRED_RESTRICTED_ROOTS
  //   SCH_CRED_REVOCATION_CHECK_CACHE_ONLY
  //   SCH_CRED_CACHE_ONLY_URL_RETRIEVAL
  //   SCH_CRED_MEMORY_STORE_CERT
  schannel_cred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS |
                           SCH_CRED_MANUAL_CRED_VALIDATION;
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
    return MapSecurityError(status);
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
    return MapSecurityError(status);
  }

  next_state_ = STATE_HANDSHAKE_WRITE;
  return OK;
}

int SSLClientSocketWin::DoHandshakeRead() {
  next_state_ = STATE_HANDSHAKE_READ_COMPLETE;

  if (!recv_buffer_.get())
    recv_buffer_.reset(new char[kRecvBufferSize]);

  char* buf = recv_buffer_.get() + bytes_received_;
  int buf_len = kRecvBufferSize - bytes_received_;

  if (buf_len <= 0) {
    NOTREACHED() << "Receive buffer is too small!";
    return ERR_UNEXPECTED;
  }

  return transport_->Read(buf, buf_len, &io_callback_);
}

int SSLClientSocketWin::DoHandshakeReadComplete(int result) {
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

  // When InitializeSecurityContext returns SEC_I_INCOMPLETE_CREDENTIALS,
  // John Banes (a Microsoft security developer) said we need to pass in the
  // ISC_REQ_USE_SUPPLIED_CREDS flag if we skip finding a client certificate
  // and just call InitializeSecurityContext again.  (See
  // (http://www.derkeiler.com/Newsgroups/microsoft.public.platformsdk.security/2004-08/0187.html.)
  // My testing on XP SP2 and Vista SP1 shows that it still works without
  // passing in this flag, but I pass it in to be safe.
  if (no_client_cert_)
    flags |= ISC_REQ_USE_SUPPLIED_CREDS;

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
    // If FAILED(status) is true, we should terminate the connection after
    // sending send_buffer_.
    if (status == SEC_E_OK)
      complete_handshake_on_write_complete_ = true;
    // We only handle these cases correctly.
    DCHECK(status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED);
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
    return MapSecurityError(status);

  if (status == SEC_I_INCOMPLETE_CREDENTIALS) {
    // We don't support SSL client authentication yet.  For now we just set
    // no_client_cert_ to true and call InitializeSecurityContext again.
    no_client_cert_ = true;
    next_state_ = STATE_HANDSHAKE_READ_COMPLETE;
    ignore_ok_result_ = true;  // OK doesn't mean EOF.
    return OK;
  }

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

int SSLClientSocketWin::DoHandshakeWrite() {
  next_state_ = STATE_HANDSHAKE_WRITE_COMPLETE;

  // We should have something to send.
  DCHECK(send_buffer_.pvBuffer);
  DCHECK(send_buffer_.cbBuffer > 0);

  const char* buf = static_cast<char*>(send_buffer_.pvBuffer) + bytes_sent_;
  int buf_len = send_buffer_.cbBuffer - bytes_sent_;

  return transport_->Write(buf, buf_len, &io_callback_);
}

int SSLClientSocketWin::DoHandshakeWriteComplete(int result) {
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
    if (complete_handshake_on_write_complete_)
      return DidCompleteHandshake();
    next_state_ = STATE_HANDSHAKE_READ;
  } else {
    // Send the remaining bytes.
    next_state_ = STATE_HANDSHAKE_WRITE;
  }

  return OK;
}

int SSLClientSocketWin::DoPayloadRead() {
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

int SSLClientSocketWin::DoPayloadReadComplete(int result) {
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
    return MapSecurityError(status);
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

  if (status == SEC_I_RENEGOTIATE) {
    // TODO(wtc): support renegotiation.
    // Should ideally send a no_renegotiation alert to the server.
    return ERR_SSL_RENEGOTIATION_REQUESTED;
  }

  // If we decrypted 0 bytes, don't report 0 bytes read, which would be
  // mistaken for EOF.  Continue decrypting or read more.
  if (len == 0) {
    if (bytes_received_ == 0) {
      next_state_ = STATE_PAYLOAD_READ;
    } else {
      next_state_ = STATE_PAYLOAD_READ_COMPLETE;
      ignore_ok_result_ = true;  // OK doesn't mean EOF.
    }
  }
  return len;
}

int SSLClientSocketWin::DoPayloadEncrypt() {
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
    return MapSecurityError(status);

  payload_send_buffer_len_ = buffers[0].cbBuffer +
                             buffers[1].cbBuffer +
                             buffers[2].cbBuffer;
  DCHECK(bytes_sent_ == 0);

  next_state_ = STATE_PAYLOAD_WRITE;
  return OK;
}

int SSLClientSocketWin::DoPayloadWrite() {
  next_state_ = STATE_PAYLOAD_WRITE_COMPLETE;

  // We should have something to send.
  DCHECK(payload_send_buffer_.get());
  DCHECK(payload_send_buffer_len_ > 0);

  const char* buf = payload_send_buffer_.get() + bytes_sent_;
  int buf_len = payload_send_buffer_len_ - bytes_sent_;

  return transport_->Write(buf, buf_len, &io_callback_);
}

int SSLClientSocketWin::DoPayloadWriteComplete(int result) {
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

int SSLClientSocketWin::DidCompleteHandshake() {
  SECURITY_STATUS status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_STREAM_SIZES, &stream_sizes_);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "QueryContextAttributes failed: " << status;
    return MapSecurityError(status);
  }
  DCHECK(!server_cert_);
  status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &server_cert_);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "QueryContextAttributes failed: " << status;
    return MapSecurityError(status);
  }

  completed_handshake_ = true;
  return VerifyServerCert();
}

// Set server_cert_status_ and return OK or a network error.
int SSLClientSocketWin::VerifyServerCert() {
  DCHECK(server_cert_);
  server_cert_status_ = 0;

  // Build and validate certificate chain.

  CERT_CHAIN_PARA chain_para;
  memset(&chain_para, 0, sizeof(chain_para));
  chain_para.cbSize = sizeof(chain_para);
  // TODO(wtc): consider requesting the usage szOID_PKIX_KP_SERVER_AUTH
  // or szOID_SERVER_GATED_CRYPTO or szOID_SGC_NETSCAPE
  chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
  chain_para.RequestedUsage.Usage.cUsageIdentifier = 0;
  chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = NULL;  // LPSTR*
  // We can set CERT_CHAIN_RETURN_LOWER_QUALITY_CONTEXTS to get more chains.
  DWORD flags = CERT_CHAIN_CACHE_END_CERT;
  if (ssl_config_.rev_checking_enabled) {
    server_cert_status_ |= CERT_STATUS_REV_CHECKING_ENABLED;
    flags |= CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  } else {
    flags |= CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;
  }
  PCCERT_CHAIN_CONTEXT chain_context;
  if (!CertGetCertificateChain(
           NULL,  // default chain engine, HCCE_CURRENT_USER
           server_cert_,
           NULL,  // current system time
           server_cert_->hCertStore,  // search this store
           &chain_para,
           flags,
           NULL,  // reserved
           &chain_context)) {
    return MapSecurityError(GetLastError());
  }
  ScopedCertChainContext scoped_chain_context(chain_context);

  server_cert_status_ |= MapCertChainErrorStatusToCertStatus(
      chain_context->TrustStatus.dwErrorStatus);

  std::wstring wstr_hostname = ASCIIToWide(hostname_);

  SSL_EXTRA_CERT_CHAIN_POLICY_PARA extra_policy_para;
  memset(&extra_policy_para, 0, sizeof(extra_policy_para));
  extra_policy_para.cbSize = sizeof(extra_policy_para);
  extra_policy_para.dwAuthType = AUTHTYPE_SERVER;
  extra_policy_para.fdwChecks = 0;
  extra_policy_para.pwszServerName =
      const_cast<wchar_t*>(wstr_hostname.c_str());

  CERT_CHAIN_POLICY_PARA policy_para;
  memset(&policy_para, 0, sizeof(policy_para));
  policy_para.cbSize = sizeof(policy_para);
  policy_para.dwFlags = 0;
  policy_para.pvExtraPolicyPara = &extra_policy_para;

  CERT_CHAIN_POLICY_STATUS policy_status;
  memset(&policy_status, 0, sizeof(policy_status));
  policy_status.cbSize = sizeof(policy_status);

  if (!CertVerifyCertificateChainPolicy(
           CERT_CHAIN_POLICY_SSL,
           chain_context,
           &policy_para,
           &policy_status)) {
    return MapSecurityError(GetLastError());
  }

  if (policy_status.dwError) {
    server_cert_status_ |= MapNetErrorToCertStatus(
        MapSecurityError(policy_status.dwError));

    // CertVerifyCertificateChainPolicy reports only one error (in
    // policy_status.dwError) if the certificate has multiple errors.
    // CertGetCertificateChain doesn't report certificate name mismatch, so
    // CertVerifyCertificateChainPolicy is the only function that can report
    // certificate name mismatch.
    //
    // To prevent a potential certificate name mismatch from being hidden by
    // some other certificate error, if we get any other certificate error,
    // we call CertVerifyCertificateChainPolicy again, ignoring all other
    // certificate errors.  Both extra_policy_para.fdwChecks and
    // policy_para.dwFlags allow us to ignore certificate errors, so we set
    // them both.
    if (policy_status.dwError != CERT_E_CN_NO_MATCH) {
      const DWORD extra_ignore_flags =
          0x00000080 |  // SECURITY_FLAG_IGNORE_REVOCATION
          0x00000100 |  // SECURITY_FLAG_IGNORE_UNKNOWN_CA
          0x00002000 |  // SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
          0x00000200;   // SECURITY_FLAG_IGNORE_WRONG_USAGE
      extra_policy_para.fdwChecks = extra_ignore_flags;
      const DWORD ignore_flags =
          CERT_CHAIN_POLICY_IGNORE_ALL_NOT_TIME_VALID_FLAGS |
          CERT_CHAIN_POLICY_IGNORE_INVALID_BASIC_CONSTRAINTS_FLAG |
          CERT_CHAIN_POLICY_ALLOW_UNKNOWN_CA_FLAG |
          CERT_CHAIN_POLICY_IGNORE_WRONG_USAGE_FLAG |
          CERT_CHAIN_POLICY_IGNORE_INVALID_NAME_FLAG |
          CERT_CHAIN_POLICY_IGNORE_INVALID_POLICY_FLAG |
          CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS |
          CERT_CHAIN_POLICY_ALLOW_TESTROOT_FLAG |
          CERT_CHAIN_POLICY_TRUST_TESTROOT_FLAG |
          CERT_CHAIN_POLICY_IGNORE_NOT_SUPPORTED_CRITICAL_EXT_FLAG |
          CERT_CHAIN_POLICY_IGNORE_PEER_TRUST_FLAG;
      policy_para.dwFlags = ignore_flags;
      if (!CertVerifyCertificateChainPolicy(
               CERT_CHAIN_POLICY_SSL,
               chain_context,
               &policy_para,
               &policy_status)) {
        return MapSecurityError(GetLastError());
      }
      if (policy_status.dwError) {
        server_cert_status_ |= MapNetErrorToCertStatus(
            MapSecurityError(policy_status.dwError));
      }
    }
  }

  if (IsCertStatusError(server_cert_status_))
    return MapCertStatusToNetError(server_cert_status_);
  return OK;
}

}  // namespace net

