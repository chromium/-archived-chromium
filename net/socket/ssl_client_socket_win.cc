// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_win.h"

#include <schnlsp.h>

#include "base/lock.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_cert_request_info.h"
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
    case SEC_E_INVALID_HANDLE:
      return ERR_UNEXPECTED;
    case SEC_E_OK:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Returns true if the two CERT_CONTEXTs contain the same certificate.
bool SameCert(PCCERT_CONTEXT a, PCCERT_CONTEXT b) {
  return a == b ||
         (a->cbCertEncoded == b->cbCertEncoded &&
         memcmp(a->pbCertEncoded, b->pbCertEncoded, b->cbCertEncoded) == 0);
}

//-----------------------------------------------------------------------------

// A bitmask consisting of these bit flags encodes which versions of the SSL
// protocol (SSL 2.0, SSL 3.0, and TLS 1.0) are enabled.
enum {
  SSL2 = 1 << 0,
  SSL3 = 1 << 1,
  TLS1 = 1 << 2,
  SSL_VERSION_MASKS = 1 << 3  // The number of SSL version bitmasks.
};

// CredHandleClass simply gives a default constructor and a destructor to
// SSPI's CredHandle type (a C struct).
class CredHandleClass : public CredHandle {
 public:
  CredHandleClass() {
    dwLower = 0;
    dwUpper = 0;
  }

  ~CredHandleClass() {
    if (dwLower || dwUpper) {
      SECURITY_STATUS status = FreeCredentialsHandle(this);
      DCHECK(status == SEC_E_OK);
    }
  }
};

// A table of CredHandles.
class CredHandleTable {
 public:
  CredHandleTable() {}

  ~CredHandleTable() {
    STLDeleteContainerPairSecondPointers(client_cert_creds_.begin(),
                                         client_cert_creds_.end());
  }

  CredHandle* GetHandle(PCCERT_CONTEXT client_cert, int ssl_version_mask) {
    DCHECK(0 < ssl_version_mask &&
           ssl_version_mask < arraysize(anonymous_creds_));
    CredHandleClass* handle;
    AutoLock lock(lock_);
    if (client_cert) {
      CredHandleMapKey key = std::make_pair(client_cert, ssl_version_mask);
      CredHandleMap::const_iterator it = client_cert_creds_.find(key);
      if (it == client_cert_creds_.end()) {
        handle = new CredHandleClass;
        client_cert_creds_[key] = handle;
      } else {
        handle = it->second;
      }
    } else {
      handle = &anonymous_creds_[ssl_version_mask];
    }
    if (!handle->dwLower && !handle->dwUpper)
      InitializeHandle(handle, client_cert, ssl_version_mask);
    return handle;
  }

 private:
  // CredHandleMapKey is a std::pair consisting of these two components:
  //   PCCERT_CONTEXT client_cert
  //   int ssl_version_mask
  typedef std::pair<PCCERT_CONTEXT, int> CredHandleMapKey;

  typedef std::map<CredHandleMapKey, CredHandleClass*> CredHandleMap;

  static void InitializeHandle(CredHandle* handle,
                               PCCERT_CONTEXT client_cert,
                               int ssl_version_mask);

  Lock lock_;

  // Anonymous (no client certificate) CredHandles for all possible
  // combinations of SSL versions.  Defined as an array for fast lookup.
  CredHandleClass anonymous_creds_[SSL_VERSION_MASKS];

  // CredHandles that use a client certificate.
  CredHandleMap client_cert_creds_;
};

// static
void CredHandleTable::InitializeHandle(CredHandle* handle,
                                       PCCERT_CONTEXT client_cert,
                                       int ssl_version_mask) {
  SCHANNEL_CRED schannel_cred = {0};
  schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
  if (client_cert) {
    schannel_cred.cCreds = 1;
    schannel_cred.paCred = &client_cert;
    // Schannel will make its own copy of client_cert.
  }

  // The global system registry settings take precedence over the value of
  // schannel_cred.grbitEnabledProtocols.
  schannel_cred.grbitEnabledProtocols = 0;
  if (ssl_version_mask & SSL2)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_SSL2;
  if (ssl_version_mask & SSL3)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_SSL3;
  if (ssl_version_mask & TLS1)
    schannel_cred.grbitEnabledProtocols |= SP_PROT_TLS1;

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
      handle,
      &expiry);  // Optional
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "AcquireCredentialsHandle failed: " << status;
    // GetHandle will return a pointer to an uninitialized CredHandle, which
    // will cause InitializeSecurityContext to fail with SEC_E_INVALID_HANDLE.
  }
}

// For the SSL sockets to share SSL sessions by session resumption handshakes,
// they need to use the same CredHandle.  The GetCredHandle function creates
// and returns a shared CredHandle.
//
// The versions of the SSL protocol enabled are a property of the CredHandle.
// So we need a separate CredHandle for each combination of SSL versions.
// Most of the time Chromium will use only one or two combinations of SSL
// versions (for example, SSL3 | TLS1 for normal use, plus SSL3 when visiting
// TLS-intolerant servers).  These CredHandles are initialized only when
// needed.

static CredHandle* GetCredHandle(PCCERT_CONTEXT client_cert,
                                 int ssl_version_mask) {
  // It doesn't matter whether GetCredHandle returns NULL or a pointer to an
  // uninitialized CredHandle on failure.  Both of them cause
  // InitializeSecurityContext to fail with SEC_E_INVALID_HANDLE.
  if (ssl_version_mask <= 0 || ssl_version_mask >= SSL_VERSION_MASKS) {
    NOTREACHED();
    return NULL;
  }
  return Singleton<CredHandleTable>::get()->GetHandle(client_cert,
                                                      ssl_version_mask);
}

//-----------------------------------------------------------------------------

// A memory certificate store for client certificates.  This allows us to
// close the "MY" system certificate store when we finish searching for
// client certificates.
class ClientCertStore {
 public:
  ClientCertStore() {
    store_ = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, NULL);
  }

  ~ClientCertStore() {
    if (store_) {
      BOOL ok = CertCloseStore(store_, CERT_CLOSE_STORE_CHECK_FLAG);
      DCHECK(ok);
    }
  }

  PCCERT_CONTEXT CopyCertContext(PCCERT_CONTEXT client_cert) {
    PCCERT_CONTEXT copy;
    BOOL ok = CertAddCertificateContextToStore(store_, client_cert,
                                               CERT_STORE_ADD_USE_EXISTING,
                                               &copy);
    DCHECK(ok);
    return ok ? copy : NULL;
  }

 private:
  HCERTSTORE store_;
};

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
      user_buf_len_(0),
      next_state_(STATE_NONE),
      creds_(NULL),
      isc_status_(SEC_E_OK),
      payload_send_buffer_len_(0),
      bytes_sent_(0),
      decrypted_ptr_(NULL),
      bytes_decrypted_(0),
      received_ptr_(NULL),
      bytes_received_(0),
      writing_first_token_(false),
      completed_handshake_(false),
      ignore_ok_result_(false),
      renegotiating_(false) {
  memset(&stream_sizes_, 0, sizeof(stream_sizes_));
  memset(in_buffers_, 0, sizeof(in_buffers_));
  memset(&send_buffer_, 0, sizeof(send_buffer_));
  memset(&ctxt_, 0, sizeof(ctxt_));
}

SSLClientSocketWin::~SSLClientSocketWin() {
  Disconnect();
}

void SSLClientSocketWin::GetSSLInfo(SSLInfo* ssl_info) {
  if (!server_cert_)
    return;

  ssl_info->cert = server_cert_;
  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  SecPkgContext_ConnectionInfo connection_info;
  SECURITY_STATUS status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_CONNECTION_INFO, &connection_info);
  if (status == SEC_E_OK) {
    // TODO(wtc): compute the overall security strength, taking into account
    // dwExchStrength and dwHashStrength.  dwExchStrength needs to be
    // normalized.
    ssl_info->security_bits = connection_info.dwCipherStrength;
  }
}

void SSLClientSocketWin::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  cert_request_info->host_and_port = hostname_;  // TODO(wtc): no port!
  cert_request_info->client_certs.clear();

  // Get the certificate_authorities field of the CertificateRequest message.
  // Schannel doesn't return the certificate_types field of the
  // CertificateRequest message to us, so we can't filter the client
  // certificates properly. :-(
  SecPkgContext_IssuerListInfoEx issuer_list;
  SECURITY_STATUS status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_ISSUER_LIST_EX, &issuer_list);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "QueryContextAttributes (issuer list) failed: " << status;
    return;
  }

  // Client certificates of the user are in the "MY" system certificate store.
  HCERTSTORE my_cert_store = CertOpenSystemStore(NULL, L"MY");
  if (!my_cert_store) {
    FreeContextBuffer(issuer_list.aIssuers);
    return;
  }

  // Enumerate the client certificates.
  CERT_CHAIN_FIND_BY_ISSUER_PARA find_by_issuer_para;
  memset(&find_by_issuer_para, 0, sizeof(find_by_issuer_para));
  find_by_issuer_para.cbSize = sizeof(find_by_issuer_para);
  find_by_issuer_para.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
  find_by_issuer_para.cIssuer = issuer_list.cIssuers;
  find_by_issuer_para.rgIssuer = issuer_list.aIssuers;

  PCCERT_CHAIN_CONTEXT chain_context = NULL;

  for (;;) {
    // Find a certificate chain.
    chain_context = CertFindChainInStore(my_cert_store,
                                         X509_ASN_ENCODING,
                                         0,
                                         CERT_CHAIN_FIND_BY_ISSUER,
                                         &find_by_issuer_para,
                                         chain_context);
    if (!chain_context) {
      DWORD err = GetLastError();
      if (err != CRYPT_E_NOT_FOUND)
        DLOG(ERROR) << "CertFindChainInStore failed: " << err;
      break;
    }

    // Get the leaf certificate.
    PCCERT_CONTEXT cert_context =
        chain_context->rgpChain[0]->rgpElement[0]->pCertContext;
    // Copy it to our own certificate store, so that we can close the "MY"
    // certificate store before returning from this function.
    PCCERT_CONTEXT cert_context2 =
        Singleton<ClientCertStore>::get()->CopyCertContext(cert_context);
    if (!cert_context2) {
      NOTREACHED();
      continue;
    }
    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
        cert_context2, X509Certificate::SOURCE_LONE_CERT_IMPORT);
    cert_request_info->client_certs.push_back(cert);
  }

  FreeContextBuffer(issuer_list.aIssuers);

  BOOL ok = CertCloseStore(my_cert_store, CERT_CLOSE_STORE_CHECK_FLAG);
  DCHECK(ok);
}

int SSLClientSocketWin::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  int ssl_version_mask = 0;
  if (ssl_config_.ssl2_enabled)
    ssl_version_mask |= SSL2;
  if (ssl_config_.ssl3_enabled)
    ssl_version_mask |= SSL3;
  if (ssl_config_.tls1_enabled)
    ssl_version_mask |= TLS1;
  // If we pass 0 to GetCredHandle, we will let Schannel select the protocols,
  // rather than enabling no protocols.  So we have to fail here.
  if (ssl_version_mask == 0)
    return ERR_NO_SSL_VERSIONS_ENABLED;
  PCCERT_CONTEXT cert_context = NULL;
  if (ssl_config_.client_cert)
    cert_context = ssl_config_.client_cert->os_cert_handle();
  creds_ = GetCredHandle(cert_context, ssl_version_mask);

  memset(&ctxt_, 0, sizeof(ctxt_));

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

  TimeStamp expiry;
  SECURITY_STATUS status;

  status = InitializeSecurityContext(
      creds_,
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

  writing_first_token_ = true;
  next_state_ = STATE_HANDSHAKE_WRITE;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void SSLClientSocketWin::Disconnect() {
  // TODO(wtc): Send SSL close_notify alert.
  completed_handshake_ = false;
  // Shut down anything that may call us back through io_callback_.
  verifier_.reset();
  transport_->Disconnect();

  if (send_buffer_.pvBuffer)
    FreeSendBuffer();
  if (ctxt_.dwLower || ctxt_.dwUpper) {
    DeleteSecurityContext(&ctxt_);
    memset(&ctxt_, 0, sizeof(ctxt_));
  }
  if (server_cert_)
    server_cert_ = NULL;

  // TODO(wtc): reset more members?
  bytes_decrypted_ = 0;
  bytes_received_ = 0;
  writing_first_token_ = false;
  renegotiating_ = false;
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

bool SSLClientSocketWin::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so transport_->IsConnectedAndIdle()
  // returns the desired false when we receive close_notify.
  return completed_handshake_ && transport_->IsConnectedAndIdle();
}

int SSLClientSocketWin::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  // If we have surplus decrypted plaintext, satisfy the Read with it without
  // reading more ciphertext from the transport socket.
  if (bytes_decrypted_ != 0) {
    int len = std::min(buf_len, bytes_decrypted_);
    memcpy(buf->data(), decrypted_ptr_, len);
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

  DCHECK(!user_buf_);
  // http://crbug.com/16371: We're seeing |buf->data()| return NULL.  See if the
  // user is passing in an IOBuffer with a NULL |data_|.
  CHECK(buf);
  CHECK(buf->data());
  user_buf_ = buf;
  user_buf_len_ = buf_len;

  SetNextStateForRead();
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_callback_ = callback;
  } else {
    user_buf_ = NULL;
  }
  return rv;
}

int SSLClientSocketWin::Write(IOBuffer* buf, int buf_len,
                              CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  DCHECK(!user_buf_);
  user_buf_ = buf;
  user_buf_len_ = buf_len;

  next_state_ = STATE_PAYLOAD_ENCRYPT;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_callback_ = callback;
  } else {
    user_buf_ = NULL;
  }
  return rv;
}

void SSLClientSocketWin::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  user_buf_ = NULL;
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
      case STATE_VERIFY_CERT:
        rv = DoVerifyCert();
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
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

int SSLClientSocketWin::DoHandshakeRead() {
  next_state_ = STATE_HANDSHAKE_READ_COMPLETE;

  if (!recv_buffer_.get())
    recv_buffer_.reset(new char[kRecvBufferSize]);

  int buf_len = kRecvBufferSize - bytes_received_;

  if (buf_len <= 0) {
    NOTREACHED() << "Receive buffer is too small!";
    return ERR_UNEXPECTED;
  }

  DCHECK(!transport_buf_);
  transport_buf_ = new IOBuffer(buf_len);

  return transport_->Read(transport_buf_, buf_len, &io_callback_);
}

int SSLClientSocketWin::DoHandshakeReadComplete(int result) {
  if (result < 0) {
    transport_buf_ = NULL;
    return result;
  }

  if (transport_buf_) {
    // A transition to STATE_HANDSHAKE_READ_COMPLETE is set in multiple places,
    // not only in DoHandshakeRead(), so we may not have a transport_buf_.
    DCHECK_LE(result, kRecvBufferSize - bytes_received_);
    char* buf = recv_buffer_.get() + bytes_received_;
    memcpy(buf, transport_buf_->data(), result);
    transport_buf_ = NULL;
  }

  if (result == 0 && !ignore_ok_result_)
    return ERR_SSL_PROTOCOL_ERROR;  // Incomplete response :(

  ignore_ok_result_ = false;

  bytes_received_ += result;

  // Process the contents of recv_buffer_.
  TimeStamp expiry;
  DWORD out_flags;

  DWORD flags = ISC_REQ_SEQUENCE_DETECT |
                ISC_REQ_REPLAY_DETECT |
                ISC_REQ_CONFIDENTIALITY |
                ISC_RET_EXTENDED_ERROR |
                ISC_REQ_ALLOCATE_MEMORY |
                ISC_REQ_STREAM;

  if (ssl_config_.send_client_cert)
    flags |= ISC_REQ_USE_SUPPLIED_CREDS;

  SecBufferDesc in_buffer_desc, out_buffer_desc;

  in_buffer_desc.cBuffers = 2;
  in_buffer_desc.pBuffers = in_buffers_;
  in_buffer_desc.ulVersion = SECBUFFER_VERSION;

  in_buffers_[0].pvBuffer = recv_buffer_.get();
  in_buffers_[0].cbBuffer = bytes_received_;
  in_buffers_[0].BufferType = SECBUFFER_TOKEN;

  in_buffers_[1].pvBuffer = NULL;
  in_buffers_[1].cbBuffer = 0;
  in_buffers_[1].BufferType = SECBUFFER_EMPTY;

  out_buffer_desc.cBuffers = 1;
  out_buffer_desc.pBuffers = &send_buffer_;
  out_buffer_desc.ulVersion = SECBUFFER_VERSION;

  send_buffer_.pvBuffer = NULL;
  send_buffer_.BufferType = SECBUFFER_TOKEN;
  send_buffer_.cbBuffer = 0;

  isc_status_ = InitializeSecurityContext(
      creds_,
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

  if (send_buffer_.cbBuffer != 0 &&
      (isc_status_ == SEC_E_OK ||
       isc_status_ == SEC_I_CONTINUE_NEEDED ||
      (FAILED(isc_status_) && (out_flags & ISC_RET_EXTENDED_ERROR)))) {
    next_state_ = STATE_HANDSHAKE_WRITE;
    return OK;
  }
  return DidCallInitializeSecurityContext();
}

int SSLClientSocketWin::DidCallInitializeSecurityContext() {
  if (isc_status_ == SEC_E_INCOMPLETE_MESSAGE) {
    next_state_ = STATE_HANDSHAKE_READ;
    return OK;
  }

  if (isc_status_ == SEC_E_OK) {
    if (in_buffers_[1].BufferType == SECBUFFER_EXTRA) {
      // Save this data for later.
      memmove(recv_buffer_.get(),
              recv_buffer_.get() + (bytes_received_ - in_buffers_[1].cbBuffer),
              in_buffers_[1].cbBuffer);
      bytes_received_ = in_buffers_[1].cbBuffer;
    } else {
      bytes_received_ = 0;
    }
    return DidCompleteHandshake();
  }

  if (FAILED(isc_status_)) {
    int result = MapSecurityError(isc_status_);
    // We told Schannel to not verify the server certificate
    // (SCH_CRED_MANUAL_CRED_VALIDATION), so any certificate error returned by
    // InitializeSecurityContext must be referring to the bad or missing
    // client certificate.
    if (IsCertificateError(result)) {
      // TODO(wtc): Add new error codes for client certificate errors reported
      // by the server using SSL/TLS alert messages.  See the MSDN page
      // "Schannel Error Codes for TLS and SSL Alerts", which maps TLS alert
      // messages to Windows error codes:
      // http://msdn.microsoft.com/en-us/library/dd721886%28VS.85%29.aspx
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    }
    return result;
  }

  if (isc_status_ == SEC_I_INCOMPLETE_CREDENTIALS)
    return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;

  DCHECK(isc_status_ == SEC_I_CONTINUE_NEEDED);
  if (in_buffers_[1].BufferType == SECBUFFER_EXTRA) {
    memmove(recv_buffer_.get(),
            recv_buffer_.get() + (bytes_received_ - in_buffers_[1].cbBuffer),
            in_buffers_[1].cbBuffer);
    bytes_received_ = in_buffers_[1].cbBuffer;
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
  DCHECK(!transport_buf_);

  const char* buf = static_cast<char*>(send_buffer_.pvBuffer) + bytes_sent_;
  int buf_len = send_buffer_.cbBuffer - bytes_sent_;
  transport_buf_ = new IOBuffer(buf_len);
  memcpy(transport_buf_->data(), buf, buf_len);

  return transport_->Write(transport_buf_, buf_len, &io_callback_);
}

int SSLClientSocketWin::DoHandshakeWriteComplete(int result) {
  DCHECK(transport_buf_);
  transport_buf_ = NULL;
  if (result < 0)
    return result;

  DCHECK(result != 0);

  bytes_sent_ += result;
  DCHECK(bytes_sent_ <= static_cast<int>(send_buffer_.cbBuffer));

  if (bytes_sent_ >= static_cast<int>(send_buffer_.cbBuffer)) {
    bool overflow = (bytes_sent_ > static_cast<int>(send_buffer_.cbBuffer));
    FreeSendBuffer();
    bytes_sent_ = 0;
    if (overflow)  // Bug!
      return ERR_UNEXPECTED;
    if (writing_first_token_) {
      writing_first_token_ = false;
      DCHECK(bytes_received_ == 0);
      next_state_ = STATE_HANDSHAKE_READ;
      return OK;
    }
    return DidCallInitializeSecurityContext();
  }

  // Send the remaining bytes.
  next_state_ = STATE_HANDSHAKE_WRITE;
  return OK;
}

// Set server_cert_status_ and return OK or a network error.
int SSLClientSocketWin::DoVerifyCert() {
  next_state_ = STATE_VERIFY_CERT_COMPLETE;

  DCHECK(server_cert_);

  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  verifier_.reset(new CertVerifier);
  return verifier_->Verify(server_cert_, hostname_, flags,
                           &server_cert_verify_result_, &io_callback_);
}

int SSLClientSocketWin::DoVerifyCertComplete(int result) {
  DCHECK(verifier_.get());
  verifier_.reset();

  // If we have been explicitly told to accept this certificate, override the
  // result of verifier_.Verify.
  // Eventually, we should cache the cert verification results so that we don't
  // need to call verifier_.Verify repeatedly. But for now we need to do this.
  // Alternatively, we might be able to store the cert's status along with
  // the cert in the allowed_bad_certs_ set.
  if (IsCertificateError(result) &&
      ssl_config_.allowed_bad_certs_.count(server_cert_))
    result = OK;

  LogConnectionTypeMetrics();
  if (renegotiating_) {
    DidCompleteRenegotiation(result);
  } else {
    // The initial handshake, kicked off by a Connect, has completed.
    completed_handshake_ = true;
    // Exit DoLoop and return the result to the caller of Connect.
    DCHECK(next_state_ == STATE_NONE);
  }
  return result;
}

int SSLClientSocketWin::DoPayloadRead() {
  next_state_ = STATE_PAYLOAD_READ_COMPLETE;

  DCHECK(recv_buffer_.get());

  int buf_len = kRecvBufferSize - bytes_received_;

  if (buf_len <= 0) {
    NOTREACHED() << "Receive buffer is too small!";
    return ERR_FAILED;
  }

  DCHECK(!transport_buf_);
  transport_buf_ = new IOBuffer(buf_len);

  return transport_->Read(transport_buf_, buf_len, &io_callback_);
}

int SSLClientSocketWin::DoPayloadReadComplete(int result) {
  if (result < 0) {
    transport_buf_ = NULL;
    return result;
  }
  if (transport_buf_) {
    // This method is called after a state transition following DoPayloadRead(),
    // or if SetNextStateForRead() was called. We have a transport_buf_ only
    // in the first case, and we have to transfer the data from transport_buf_
    // to recv_buffer_.
    DCHECK_LE(result, kRecvBufferSize - bytes_received_);
    char* buf = recv_buffer_.get() + bytes_received_;
    memcpy(buf, transport_buf_->data(), result);
    transport_buf_ = NULL;
  }

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
    memcpy(user_buf_->data(), decrypted_ptr_, len);
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
    if (bytes_received_ != 0) {
      // The server requested renegotiation, but there are some data yet to
      // be decrypted.  The Platform SDK WebClient.c sample doesn't handle
      // this, so we don't know how to handle this.  Assume this cannot
      // happen.
      LOG(ERROR) << "DecryptMessage returned SEC_I_RENEGOTIATE with a buffer "
                 << "of type SECBUFFER_EXTRA.";
      return ERR_SSL_RENEGOTIATION_REQUESTED;
    }
    if (len != 0) {
      // The server requested renegotiation, but there are some decrypted
      // data.  We can't start renegotiation until we have returned all
      // decrypted data to the caller.
      //
      // This hasn't happened during testing.  Assume this cannot happen even
      // though we know how to handle this.
      LOG(ERROR) << "DecryptMessage returned SEC_I_RENEGOTIATE with a buffer "
                 << "of type SECBUFFER_DATA.";
      return ERR_SSL_RENEGOTIATION_REQUESTED;
    }
    // Jump to the handshake sequence.  Will come back when the rehandshake is
    // done.
    renegotiating_ = true;
    next_state_ = STATE_HANDSHAKE_READ_COMPLETE;
    ignore_ok_result_ = true;  // OK doesn't mean EOF.
    return len;
  }

  // If we decrypted 0 bytes, don't report 0 bytes read, which would be
  // mistaken for EOF.  Continue decrypting or read more.
  if (len == 0)
    SetNextStateForRead();
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
         user_buf_->data(), message_len);

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
  DCHECK(!transport_buf_);

  const char* buf = payload_send_buffer_.get() + bytes_sent_;
  int buf_len = payload_send_buffer_len_ - bytes_sent_;
  transport_buf_ = new IOBuffer(buf_len);
  memcpy(transport_buf_->data(), buf, buf_len);

  return transport_->Write(transport_buf_, buf_len, &io_callback_);
}

int SSLClientSocketWin::DoPayloadWriteComplete(int result) {
  DCHECK(transport_buf_);
  transport_buf_ = NULL;
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
    DLOG(ERROR) << "QueryContextAttributes (stream sizes) failed: " << status;
    return MapSecurityError(status);
  }
  DCHECK(!server_cert_ || renegotiating_);
  PCCERT_CONTEXT server_cert_handle = NULL;
  status = QueryContextAttributes(
      &ctxt_, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &server_cert_handle);
  if (status != SEC_E_OK) {
    DLOG(ERROR) << "QueryContextAttributes (remote cert) failed: " << status;
    return MapSecurityError(status);
  }
  if (renegotiating_ &&
      SameCert(server_cert_->os_cert_handle(), server_cert_handle)) {
    // We already verified the server certificate.  Either it is good or the
    // user has accepted the certificate error.
    CertFreeCertificateContext(server_cert_handle);
    DidCompleteRenegotiation(OK);
  } else {
    server_cert_ = X509Certificate::CreateFromHandle(
        server_cert_handle, X509Certificate::SOURCE_FROM_NETWORK);

    next_state_ = STATE_VERIFY_CERT;
  }
  return OK;
}

// Called when a renegotiation is completed.  |result| is the verification
// result of the server certificate received during renegotiation.
void SSLClientSocketWin::DidCompleteRenegotiation(int result) {
  // A rehandshake, started in the middle of a Read, has completed.
  renegotiating_ = false;
  // Pick up where we left off.  Go back to reading data.
  if (result == OK)
    SetNextStateForRead();
}

void SSLClientSocketWin::LogConnectionTypeMetrics() const {
  UpdateConnectionTypeHistograms(CONNECTION_SSL);
  if (server_cert_verify_result_.has_md5)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD5);
  if (server_cert_verify_result_.has_md2)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD2);
  if (server_cert_verify_result_.has_md4)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD4);
  if (server_cert_verify_result_.has_md5_ca)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD5_CA);
  if (server_cert_verify_result_.has_md2_ca)
    UpdateConnectionTypeHistograms(CONNECTION_SSL_MD2_CA);
}

void SSLClientSocketWin::SetNextStateForRead() {
  if (bytes_received_ == 0) {
    next_state_ = STATE_PAYLOAD_READ;
  } else {
    next_state_ = STATE_PAYLOAD_READ_COMPLETE;
    ignore_ok_result_ = true;  // OK doesn't mean EOF.
  }
}

void SSLClientSocketWin::FreeSendBuffer() {
  SECURITY_STATUS status = FreeContextBuffer(send_buffer_.pvBuffer);
  DCHECK(status == SEC_E_OK);
  memset(&send_buffer_, 0, sizeof(send_buffer_));
}

}  // namespace net
