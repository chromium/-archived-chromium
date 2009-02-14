// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_client_socket_nss.h"

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

#include "base/logging.h"
#include "base/nss_init.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"

static const int kRecvBufferSize = 4096;

namespace {

// NSS calls this if an incoming certificate is invalid.
SECStatus OwnBadCertHandler(void* arg, PRFileDesc* socket) {
  PRErrorCode err = PR_GetError();
  LOG(INFO) << "server certificate is invalid; NSS error code " << err;
  // Return SECSuccess to override the problem,
  // or SECFailure to let the original function fail
  // Chromium wants it to fail here, and may retry it later.
  LOG(WARNING) << "TODO(dkegel): return SECFailure here";
  return SECSuccess;
}

}  // anonymous namespace

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
    case SEC_ERROR_UNKNOWN_ISSUER:
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
      user_callback_(NULL),
      user_buf_(NULL),
      user_buf_len_(0),
      server_cert_status_(0),
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

  GotoState(STATE_CONNECT);
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;

  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::ReconnectIgnoringLastError(
        CompletionCallback* callback) {
  EnterFunction("");
  // TODO(darin): implement me!
  LeaveFunction("");
  return ERR_FAILED;
}

void SSLClientSocketNSS::Disconnect() {
  EnterFunction("");
  // TODO(wtc): Send SSL close_notify alert.
  if (nss_fd_ != NULL) {
    PR_Close(nss_fd_);
    nss_fd_ = NULL;
  }
  completed_handshake_ = false;
  transport_->Disconnect();
  LeaveFunction("");
}

bool SSLClientSocketNSS::IsConnected() const {
  EnterFunction("");
  bool ret = completed_handshake_ && transport_->IsConnected();
  LeaveFunction("");
  return ret;
}

int SSLClientSocketNSS::Read(char* buf, int buf_len,
                             CompletionCallback* callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);
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

int SSLClientSocketNSS::Write(const char* buf, int buf_len,
                           CompletionCallback* callback) {
  EnterFunction(buf_len);
  DCHECK(completed_handshake_);
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);
  DCHECK(!user_buf_);

  user_buf_ = const_cast<char*>(buf);
  user_buf_len_ = buf_len;

  GotoState(STATE_PAYLOAD_WRITE);
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  LeaveFunction(rv);
  return rv;
}

void SSLClientSocketNSS::GetSSLInfo(SSLInfo* ssl_info) {
  EnterFunction("");
  ssl_info->Reset();
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
  }
  ssl_info->cert_status = server_cert_status_;
  // TODO(port): implement X509Certificate so we can set the cert field!
  // CERTCertificate *nssCert = SSL_PeerCertificate(nss_fd_);
  LeaveFunction("");
}

void SSLClientSocketNSS::DoCallback(int rv) {
  EnterFunction(rv);
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
  LeaveFunction("");
}

void SSLClientSocketNSS::OnIOComplete(int result) {
  EnterFunction(result);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING && user_callback_ != NULL)
    DoCallback(rv);
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

/*
 * Do network I/O between the given buffer and the given socket.
 * Return 0 for EOF,
 * > 0 for bytes transferred immediately,
 * < 0 for error (or the non-error ERR_IO_PENDING).
 */
int SSLClientSocketNSS::BufferSend(void) {
  if (transport_send_busy_) return ERR_IO_PENDING;

  const char *buf;
  int nb = memio_GetWriteParams(nss_bufs_, &buf);
  EnterFunction(nb);

  int rv;
  if (!nb) {
    rv = OK;
  } else {
    rv = transport_->Write(buf, nb, &buffer_send_callback_);
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
    rv = transport_->Read(buf, nb, &buffer_recv_callback_);
    if (rv == ERR_IO_PENDING)
      transport_recv_busy_ = true;
    else
      memio_PutReadResult(nss_bufs_, MapErrorToNSS(rv));
  }
  LeaveFunction(rv);
  return rv;
}

void SSLClientSocketNSS::BufferRecvComplete(int result) {
  EnterFunction(result);
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
      case STATE_CONNECT:
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      case STATE_HANDSHAKE_READ:
        rv = DoHandshakeRead();
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
  } while ((rv != ERR_IO_PENDING || network_moved) && next_state_ != STATE_NONE);
  LeaveFunction("");
  return rv;
}

int SSLClientSocketNSS::DoConnect() {
  EnterFunction("");
  GotoState(STATE_CONNECT_COMPLETE);
  return transport_->Connect(&io_callback_);
}

int SSLClientSocketNSS::DoConnectComplete(int result) {
  EnterFunction(result);
  if (result < 0)
    return result;

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

  rv = SSL_BadCertHook(nss_fd_, OwnBadCertHandler, NULL);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  // Tell SSL the hostname we're trying to connect to.
  SSL_SetURL(nss_fd_, hostname_.c_str());

  // Tell SSL we're a client; needed if not letting NSPR do socket I/O
  SSL_ResetHandshake(nss_fd_, 0);
  GotoState(STATE_HANDSHAKE_READ);
  // Return OK so DoLoop tries handshaking
  LeaveFunction("");
  return OK;
}

int SSLClientSocketNSS::DoHandshakeRead() {
  EnterFunction("");
  int net_error;
  int rv = SSL_ForceHandshake(nss_fd_);

  if (rv == SECSuccess) {
    net_error = OK;
    // there's a callback for this, too
    completed_handshake_ = true;
    // Indicate we're ready to handle I/O.  Badly named?
    GotoState(STATE_NONE);
  } else {
    PRErrorCode prerr = PR_GetError();
    net_error = NetErrorFromNSPRError(prerr);

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE_READ);
    } else {
      server_cert_status_ = MapNetErrorToCertStatus(net_error);
      LOG(ERROR) << "handshake failed; NSS error code " << prerr
                 << ", net_error " << net_error << ", server_cert_status "
                 << server_cert_status_;
    }
  }

  LeaveFunction("");
  return net_error;
}

int SSLClientSocketNSS::DoPayloadRead() {
  EnterFunction(user_buf_len_);
  int rv = PR_Read(nss_fd_, user_buf_, user_buf_len_);
  if (rv >= 0) {
    LogData(user_buf_, rv);
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
  int rv = PR_Write(nss_fd_, user_buf_, user_buf_len_);
  if (rv >= 0) {
    LogData(user_buf_, rv);
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

