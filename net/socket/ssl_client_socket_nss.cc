// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file includes code GetDefaultCertNickname(), derived from
// nsNSSCertificate::defaultServerNickName()
// in mozilla/security/manager/ssl/src/nsNSSCertificate.cpp
// and SSLClientSocketNSS::DoVerifyCertComplete() derived from
// AuthCertificateCallback() in
// mozilla/security/manager/ssl/src/nsNSSCallbacks.cpp.

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/socket/ssl_client_socket_nss.h"

#include <certdb.h>
#include <nspr.h>
#include <nss.h>
#include <secerr.h>
// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <ssl.h>
#include <sslerr.h>
#include <pk11pub.h>
#undef Lock

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/nss_init.h"
#include "base/string_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"

static const int kRecvBufferSize = 4096;

namespace net {

// State machines are easier to debug if you log state transitions.
// Enable these if you want to see what's going on.
#if 1
#define EnterFunction(x)
#define LeaveFunction(x)
#define GotoState(s) next_state_ = s
#define LogData(s, len)
#else
#define EnterFunction(x)  LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " enter " << x << "; next_state " << next_state_
#define LeaveFunction(x)  LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " leave " << x << "; next_state " << next_state_
#define GotoState(s) do { LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " jump to state " << s; next_state_ = s; } while (0)
#define LogData(s, len)   LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " data [" << std::string(s, len) << "]";

#endif

namespace {

// Gets default certificate nickname from cert.
// Derived from nsNSSCertificate::defaultServerNickname
// in mozilla/security/manager/ssl/src/nsNSSCertificate.cpp.
std::string GetDefaultCertNickname(
    net::X509Certificate::OSCertHandle cert) {
  if (cert == NULL)
    return "";

  char* name = CERT_GetCommonName(&cert->subject);
  if (!name) {
    // Certs without common names are strange, but they do exist...
    // Let's try to use another string for the nickname
    name = CERT_GetOrgUnitName(&cert->subject);
    if (!name)
      name = CERT_GetOrgName(&cert->subject);
    if (!name)
      name = CERT_GetLocalityName(&cert->subject);
    if (!name)
      name = CERT_GetStateName(&cert->subject);
    if (!name)
      name = CERT_GetCountryName(&cert->subject);
    if (!name)
      return "";
  }
  int count = 1;
  std::string nickname;
  while (1) {
    if (count == 1) {
      nickname = name;
    } else {
      nickname = StringPrintf("%s #%d", name, count);
    }
    PRBool conflict = SEC_CertNicknameConflict(
        const_cast<char*>(nickname.c_str()), &cert->derSubject, cert->dbhandle);
    if (!conflict)
      break;
    count++;
  }
  PR_FREEIF(name);
  return nickname;
}

int NetErrorFromNSPRError(PRErrorCode err) {
  // TODO(port): fill this out as we learn what's important
  switch (err) {
    case PR_WOULD_BLOCK_ERROR:
      return ERR_IO_PENDING;
    case SSL_ERROR_NO_CYPHER_OVERLAP:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_ERROR_BAD_CERT_DOMAIN:
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return ERR_CERT_DATE_INVALID;
    case SEC_ERROR_BAD_SIGNATURE:
      return ERR_CERT_INVALID;
    case SSL_ERROR_REVOKED_CERT_ALERT:
    case SEC_ERROR_REVOKED_CERTIFICATE:
    case SEC_ERROR_REVOKED_KEY:
      return ERR_CERT_REVOKED;
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_CERT:
    case SEC_ERROR_UNTRUSTED_ISSUER:
      return ERR_CERT_AUTHORITY_INVALID;

    default: {
      if (IS_SSL_ERROR(err)) {
        LOG(WARNING) << "Unknown SSL error " << err <<
            " mapped to net::ERR_SSL_PROTOCOL_ERROR";
        return ERR_SSL_PROTOCOL_ERROR;
      }
      if (IS_SEC_ERROR(err)) {
        // TODO(port): Probably not the best mapping
        LOG(WARNING) << "Unknown SEC error " << err <<
            " mapped to net::ERR_CERT_INVALID";
        return ERR_CERT_INVALID;
      }
      LOG(WARNING) << "Unknown error " << err <<
          " mapped to net::ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

}  // namespace

bool SSLClientSocketNSS::nss_options_initialized_ = false;

SSLClientSocketNSS::SSLClientSocketNSS(ClientSocket* transport_socket,
                                       const std::string& hostname,
                                       const SSLConfig& ssl_config)
    :
      buffer_send_callback_(this, &SSLClientSocketNSS::BufferSendComplete),
      buffer_recv_callback_(this, &SSLClientSocketNSS::BufferRecvComplete),
      transport_send_busy_(false),
      transport_recv_busy_(false),
      io_callback_(this, &SSLClientSocketNSS::OnIOComplete),
      transport_(transport_socket),
      hostname_(hostname),
      ssl_config_(ssl_config),
      user_connect_callback_(NULL),
      user_callback_(NULL),
      user_buf_len_(0),
      completed_handshake_(false),
      next_state_(STATE_NONE),
      nss_fd_(NULL),
      nss_bufs_(NULL) {
  EnterFunction("");
}

SSLClientSocketNSS::~SSLClientSocketNSS() {
  EnterFunction("");
  Disconnect();
  LeaveFunction("");
}

int SSLClientSocketNSS::Init() {
  EnterFunction("");
  // Call NSS_NoDB_Init() in a threadsafe way.
  base::EnsureNSSInit();

  LeaveFunction("");
  return OK;
}

int SSLClientSocketNSS::Connect(CompletionCallback* callback) {
  EnterFunction("");
  DCHECK(transport_.get());
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_buf_);

  if (Init() != OK) {
    NOTREACHED() << "Couldn't initialize nss";
  }

  // Transport connected, now hook it up to nss
  // TODO(port): specify rx and tx buffer sizes separately
  nss_fd_ = memio_CreateIOLayer(kRecvBufferSize);
  if (nss_fd_ == NULL) {
    return 9999;  // TODO(port): real error
  }

  // Tell NSS who we're connected to
  PRNetAddr peername;
  socklen_t len = sizeof(PRNetAddr);
  int err = transport_->GetPeerName((struct sockaddr *)&peername, &len);
  if (err) {
    DLOG(ERROR) << "GetPeerName failed";
    return 9999;  // TODO(port): real error
  }
  memio_SetPeerName(nss_fd_, &peername);

  // Grab pointer to buffers
  nss_bufs_ = memio_GetSecret(nss_fd_);

  /* Create SSL state machine */
  /* Push SSL onto our fake I/O socket */
  nss_fd_ = SSL_ImportFD(NULL, nss_fd_);
  if (nss_fd_ == NULL) {
      return ERR_SSL_PROTOCOL_ERROR;  // TODO(port): real error
  }
  // TODO(port): set more ssl options!  Check errors!

  int rv;

  rv = SSL_OptionSet(nss_fd_, SSL_SECURITY, PR_TRUE);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL2, ssl_config_.ssl2_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  // SNI is enabled automatically if TLS is enabled -- as long as
  // SSL_V2_COMPATIBLE_HELLO isn't.
  // So don't do V2 compatible hellos unless we're really using SSL2,
  // to avoid errors like
  // "common name `mail.google.com' != requested host name `gmail.com'"
  rv = SSL_OptionSet(nss_fd_, SSL_V2_COMPATIBLE_HELLO,
                     ssl_config_.ssl2_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL3, ssl_config_.ssl3_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_TLS, ssl_config_.tls1_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

#ifdef SSL_ENABLE_SESSION_TICKETS
  // Support RFC 5077
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SESSION_TICKETS, PR_TRUE);
  if (rv != SECSuccess)
     LOG(INFO) << "SSL_ENABLE_SESSION_TICKETS failed.  Old system nss?";
#else
  #error "You need to install NSS-3.12 or later to build chromium"
#endif

  rv = SSL_OptionSet(nss_fd_, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_AuthCertificateHook(nss_fd_, OwnAuthCertHandler, this);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_HandshakeCallback(nss_fd_, HandshakeCallback, this);
  if (rv != SECSuccess)
    return ERR_UNEXPECTED;

  // Tell SSL the hostname we're trying to connect to.
  SSL_SetURL(nss_fd_, hostname_.c_str());

  // Tell SSL we're a client; needed if not letting NSPR do socket I/O
  SSL_ResetHandshake(nss_fd_, 0);

  GotoState(STATE_HANDSHAKE_READ);
  rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_connect_callback_ = callback;

  LeaveFunction("");
  return rv > OK ? OK : rv;
}

void SSLClientSocketNSS::InvalidateSessionIfBadCertificate() {
  if (UpdateServerCert() != NULL &&
      ssl_config_.allowed_bad_certs_.count(server_cert_)) {
    SSL_InvalidateSession(nss_fd_);
  }
}

void SSLClientSocketNSS::Disconnect() {
  EnterFunction("");

  // TODO(wtc): Send SSL close_notify alert.
  if (nss_fd_ != NULL) {
    InvalidateSessionIfBadCertificate();
    PR_Close(nss_fd_);
    nss_fd_ = NULL;
  }

  // Shut down anything that may call us back (through buffer_send_callback_,
  // buffer_recv_callback, or io_callback_).
  verifier_.reset();
  transport_->Disconnect();

  // Reset object state
  transport_send_busy_   = false;
  transport_recv_busy_   = false;
  user_connect_callback_ = NULL;
  user_callback_         = NULL;
  user_buf_              = NULL;
  user_buf_len_          = 0;
  server_cert_           = NULL;
  server_cert_verify_result_.Reset();
  completed_handshake_   = false;
  nss_bufs_              = NULL;

  LeaveFunction("");
}

bool SSLClientSocketNSS::IsConnected() const {
  // Ideally, we should also check if we have received the close_notify alert
  // message from the server, and return false in that case.  We're not doing
  // that, so this function may return a false positive.  Since the upper
  // layer (HttpNetworkTransaction) needs to handle a persistent connection
  // closed by the server when we send a request anyway, a false positive in
  // exchange for simpler code is a good trade-off.
  EnterFunction("");
  bool ret = completed_handshake_ && transport_->IsConnected();
  LeaveFunction("");
  return ret;
}

bool SSLClientSocketNSS::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so transport_->IsConnectedAndIdle()
  // returns the desired false when we receive close_notify.
  EnterFunction("");
  bool ret = completed_handshake_ && transport_->IsConnectedAndIdle();
  LeaveFunction("");
  return ret;
}

int SSLClientSocketNSS::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_buf_);

  user_buf_ = buf;
  user_buf_len_ = buf_len;

  GotoState(STATE_PAYLOAD_READ);
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  LeaveFunction(rv);
  return rv;
}

int SSLClientSocketNSS::Write(IOBuffer* buf, int buf_len,
                           CompletionCallback* callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);
  DCHECK(!user_connect_callback_);
  DCHECK(!user_buf_);

  user_buf_ = buf;
  user_buf_len_ = buf_len;

  GotoState(STATE_PAYLOAD_WRITE);
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  LeaveFunction(rv);
  return rv;
}

X509Certificate *SSLClientSocketNSS::UpdateServerCert() {
  // We set the server_cert_ from OwnAuthCertHandler(), but this handler
  // does not necessarily get called if we are continuing a cached SSL
  // session.
  if (server_cert_ == NULL) {
    X509Certificate::OSCertHandle nss_cert = SSL_PeerCertificate(nss_fd_);
    if (nss_cert) {
      server_cert_ = X509Certificate::CreateFromHandle(
          nss_cert, X509Certificate::SOURCE_FROM_NETWORK);
    }
  }
  return server_cert_;
}

void SSLClientSocketNSS::GetSSLInfo(SSLInfo* ssl_info) {
  EnterFunction("");
  ssl_info->Reset();
  if (!server_cert_)
    return;

  SSLChannelInfo channel_info;
  SECStatus ok = SSL_GetChannelInfo(nss_fd_,
                                    &channel_info, sizeof(channel_info));
  if (ok == SECSuccess &&
      channel_info.length == sizeof(channel_info) &&
      channel_info.cipherSuite) {
    SSLCipherSuiteInfo cipher_info;
    ok = SSL_GetCipherSuiteInfo(channel_info.cipherSuite,
                                &cipher_info, sizeof(cipher_info));
    if (ok == SECSuccess) {
      ssl_info->security_bits = cipher_info.effectiveKeyBits;
    } else {
      ssl_info->security_bits = -1;
      LOG(DFATAL) << "SSL_GetCipherSuiteInfo returned " << PR_GetError()
                  << " for cipherSuite " << channel_info.cipherSuite;
    }
    UpdateServerCert();
  }
  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  DCHECK(server_cert_ != NULL);
  ssl_info->cert = server_cert_;
  LeaveFunction("");
}

void SSLClientSocketNSS::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  // TODO(wtc): implement this.
}

void SSLClientSocketNSS::DoCallback(int rv) {
  EnterFunction(rv);
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // Since Run may result in Read being called, clear |user_callback_| up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  user_buf_ = NULL;
  c->Run(rv);
  LeaveFunction("");
}

// As part of Connect(), the SSLClientSocketNSS object performs an SSL
// handshake. This requires network IO, which in turn calls
// BufferRecvComplete() with a non-zero byte count. This byte count eventually
// winds its way through the state machine and ends up being passed to the
// callback. For Read() and Write(), that's what we want. But for Connect(),
// the caller expects OK (i.e. 0) for success.
//
void SSLClientSocketNSS::DoConnectCallback(int rv) {
  EnterFunction(rv);
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(user_connect_callback_);

  // Since Run may result in Read being called, clear |user_connect_callback_|
  // up front.
  CompletionCallback* c = user_connect_callback_;
  user_connect_callback_ = NULL;
  c->Run(rv > OK ? OK : rv);
  LeaveFunction("");
}

void SSLClientSocketNSS::OnIOComplete(int result) {
  EnterFunction(result);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    if (user_callback_) {
      DoCallback(rv);
    } else if (user_connect_callback_) {
      DoConnectCallback(rv);
    }
  }
  LeaveFunction("");
}

// Map a Chromium net error code to an NSS error code
// See _MD_unix_map_default_error in the NSS source
// tree for inspiration.
static PRErrorCode MapErrorToNSS(int result) {
  if (result >=0)
    return result;
  // TODO(port): add real table
  LOG(ERROR) << "MapErrorToNSS " << result;
  return PR_UNKNOWN_ERROR;
}

// Do network I/O between the given buffer and the given socket.
// Return 0 for EOF,
// > 0 for bytes transferred immediately,
// < 0 for error (or the non-error ERR_IO_PENDING).
int SSLClientSocketNSS::BufferSend(void) {
  if (transport_send_busy_) return ERR_IO_PENDING;

  const char *buf;
  int nb = memio_GetWriteParams(nss_bufs_, &buf);
  EnterFunction(nb);

  int rv;
  if (!nb) {
    rv = OK;
  } else {
    scoped_refptr<IOBuffer> send_buffer = new IOBuffer(nb);
    memcpy(send_buffer->data(), buf, nb);
    rv = transport_->Write(send_buffer, nb, &buffer_send_callback_);
    if (rv == ERR_IO_PENDING)
      transport_send_busy_ = true;
    else
      memio_PutWriteResult(nss_bufs_, MapErrorToNSS(rv));
  }

  LeaveFunction(rv);
  return rv;
}

void SSLClientSocketNSS::BufferSendComplete(int result) {
  EnterFunction(result);
  memio_PutWriteResult(nss_bufs_, result);
  transport_send_busy_ = false;
  OnIOComplete(result);
  LeaveFunction("");
}


int SSLClientSocketNSS::BufferRecv(void) {
  if (transport_recv_busy_) return ERR_IO_PENDING;

  char *buf;
  int nb = memio_GetReadParams(nss_bufs_, &buf);
  EnterFunction(nb);
  int rv;
  if (!nb) {
    // buffer too full to read into, so no I/O possible at moment
    rv = ERR_IO_PENDING;
  } else {
    recv_buffer_ = new IOBuffer(nb);
    rv = transport_->Read(recv_buffer_, nb, &buffer_recv_callback_);
    if (rv == ERR_IO_PENDING) {
      transport_recv_busy_ = true;
    } else {
      if (rv > 0)
        memcpy(buf, recv_buffer_->data(), rv);
      memio_PutReadResult(nss_bufs_, MapErrorToNSS(rv));
      recv_buffer_ = NULL;
    }
  }
  LeaveFunction(rv);
  return rv;
}

void SSLClientSocketNSS::BufferRecvComplete(int result) {
  EnterFunction(result);
  if (result > 0) {
    char *buf;
    memio_GetReadParams(nss_bufs_, &buf);
    memcpy(buf, recv_buffer_->data(), result);
  }
  recv_buffer_ = NULL;
  memio_PutReadResult(nss_bufs_, result);
  transport_recv_busy_ = false;
  OnIOComplete(result);
  LeaveFunction("");
}

int SSLClientSocketNSS::DoLoop(int last_io_result) {
  EnterFunction(last_io_result);
  bool network_moved;
  int rv = last_io_result;
  do {
    network_moved = false;
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_state_;
    GotoState(STATE_NONE);
    switch (state) {
      case STATE_NONE:
        // we're just pumping data between the buffer and the network
        break;
      case STATE_HANDSHAKE_READ:
        rv = DoHandshakeRead();
        break;
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_PAYLOAD_READ:
        rv = DoPayloadRead();
        break;
      case STATE_PAYLOAD_WRITE:
        rv = DoPayloadWrite();
        break;
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state";
        break;
    }

    // Do the actual network I/O
    if (nss_bufs_ != NULL) {
      int nsent = BufferSend();
      int nreceived = BufferRecv();
      network_moved = (nsent > 0 || nreceived >= 0);
    }
  } while ((rv != ERR_IO_PENDING || network_moved) &&
            next_state_ != STATE_NONE);
  LeaveFunction("");
  return rv;
}

// static
// NSS calls this if an incoming certificate needs to be verified.
// Do nothing but return SECSuccess.
// This is called only in full handshake mode.
// Peer certificate is retrieved in HandshakeCallback() later, which is called
// in full handshake mode or in resumption handshake mode.
SECStatus SSLClientSocketNSS::OwnAuthCertHandler(void* arg,
                                                 PRFileDesc* socket,
                                                 PRBool checksig,
                                                 PRBool is_server) {
  // Tell NSS to not verify the certificate.
  return SECSuccess;
}

// static
// NSS calls this when handshake is completed.
// After the SSL handshake is finished, use CertVerifier to verify
// the saved server certificate.
void SSLClientSocketNSS::HandshakeCallback(PRFileDesc* socket,
                                           void* arg) {
  SSLClientSocketNSS* that = reinterpret_cast<SSLClientSocketNSS*>(arg);

  that->UpdateServerCert();
}

int SSLClientSocketNSS::DoHandshakeRead() {
  EnterFunction("");
  int net_error = net::OK;
  int rv = SSL_ForceHandshake(nss_fd_);

  if (rv == SECSuccess) {
    // SSL handshake is completed.  Let's verify the certificate.
    GotoState(STATE_VERIFY_CERT);
    // Done!
  } else {
    PRErrorCode prerr = PR_GetError();

    // If the server closed on us, it is a protocol error.
    // Some TLS-intolerant servers do this when we request TLS.
    if (prerr == PR_END_OF_FILE_ERROR) {
      net_error = ERR_SSL_PROTOCOL_ERROR;
    } else {
      net_error = NetErrorFromNSPRError(prerr);
    }

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE_READ);
    } else {
      LOG(ERROR) << "handshake failed; NSS error code " << prerr
                 << ", net_error " << net_error;
    }
  }

  LeaveFunction("");
  return net_error;
}

int SSLClientSocketNSS::DoVerifyCert(int result) {
  DCHECK(server_cert_);
  GotoState(STATE_VERIFY_CERT_COMPLETE);
  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  verifier_.reset(new CertVerifier);
  return verifier_->Verify(server_cert_, hostname_, flags,
                           &server_cert_verify_result_, &io_callback_);
}

// Derived from AuthCertificateCallback() in
// mozilla/source/security/manager/ssl/src/nsNSSCallbacks.cpp.
int SSLClientSocketNSS::DoVerifyCertComplete(int result) {
  DCHECK(verifier_.get());
  verifier_.reset();

  if (result == OK) {
    // Remember the intermediate CA certs if the server sends them to us.
    CERTCertList* cert_list = CERT_GetCertChainFromCert(
        server_cert_->os_cert_handle(), PR_Now(), certUsageSSLCA);
    if (cert_list) {
      for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
           !CERT_LIST_END(node, cert_list);
           node = CERT_LIST_NEXT(node)) {
        if (node->cert->slot || node->cert->isRoot || node->cert->isperm ||
            node->cert == server_cert_->os_cert_handle()) {
          // Some certs we don't want to remember are:
          // - found on a token.
          // - the root cert.
          // - already stored in perm db.
          // - the server cert itself.
          continue;
        }

        // We have found a CA cert that we want to remember.
        std::string nickname(GetDefaultCertNickname(node->cert));
        if (!nickname.empty()) {
          PK11SlotInfo* slot = PK11_GetInternalKeySlot();
          if (slot) {
            PK11_ImportCert(slot, node->cert, CK_INVALID_HANDLE,
                            const_cast<char*>(nickname.c_str()), PR_FALSE);
            PK11_FreeSlot(slot);
          }
        }
      }
      CERT_DestroyCertList(cert_list);
    }
  }

  // If we have been explicitly told to accept this certificate, override the
  // result of verifier_.Verify.
  // Eventually, we should cache the cert verification results so that we don't
  // need to call verifier_.Verify repeatedly.  But for now we need to do this.
  // Alternatively, we might be able to store the cert's status along with
  // the cert in the allowed_bad_certs_ set.
  if (IsCertificateError(result) &&
      ssl_config_.allowed_bad_certs_.count(server_cert_)) {
    LOG(INFO) << "accepting bad SSL certificate, as user told us to";
    result = OK;
  }

  completed_handshake_ = true;
  // TODO(ukai): we may not need this call because it is now harmless to have an
  // session with a bad cert.
  InvalidateSessionIfBadCertificate();
  // Exit DoLoop and return the result to the caller to Connect.
  DCHECK(next_state_ == STATE_NONE);
  return result;
}

int SSLClientSocketNSS::DoPayloadRead() {
  EnterFunction(user_buf_len_);
  int rv = PR_Read(nss_fd_, user_buf_->data(), user_buf_len_);
  if (rv >= 0) {
    LogData(user_buf_->data(), rv);
    user_buf_ = NULL;
    LeaveFunction("");
    return rv;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    GotoState(STATE_PAYLOAD_READ);
    LeaveFunction("");
    return ERR_IO_PENDING;
  }
  user_buf_ = NULL;
  LeaveFunction("");
  return NetErrorFromNSPRError(prerr);
}

int SSLClientSocketNSS::DoPayloadWrite() {
  EnterFunction(user_buf_len_);
  int rv = PR_Write(nss_fd_, user_buf_->data(), user_buf_len_);
  if (rv >= 0) {
    LogData(user_buf_->data(), rv);
    user_buf_ = NULL;
    LeaveFunction("");
    return rv;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    GotoState(STATE_PAYLOAD_WRITE);
    return ERR_IO_PENDING;
  }
  user_buf_ = NULL;
  LeaveFunction("");
  return NetErrorFromNSPRError(prerr);
}

}  // namespace net
