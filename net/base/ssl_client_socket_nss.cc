// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_client_socket_nss.h"

#include <nspr.h>
#include <nss.h>
// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <ssl.h>
#include <pk11pub.h>
#undef Lock

#include "base/logging.h"
#include "base/nss_init.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"

static const int kRecvBufferSize = 4096;

/*
 * nss calls this if an incoming certificate is invalid.
 * TODO(port): expose to app via GetSSLInfo so it can put up 
 * the appropriate GUI and retry with override if desired
 */
static SECStatus
ownBadCertHandler(void * arg, PRFileDesc * socket)
{
    PRErrorCode err = PR_GetError();
    LOG(ERROR) << "server certificate is invalid; NSS error code " << err;
    // Return SECSuccess to override the problem, SECFailure to let the original function fail
    return SECSuccess;  /* override, say it's OK. */
}


namespace net {

// State machines are easier to debug if you log state transitions.
// Enable these if you want to see what's going on.
#if 1
#define EnterFunction(x)
#define LeaveFunction(x)
#define GotoState(s) next_state_ = s
#else
#define EnterFunction(x)  LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " enter " << x << "; next_state " << next_state_
#define LeaveFunction(x)  LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " leave " << x << "; next_state " << next_state_
#define GotoState(s) do { LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " jump to state " << s; next_state_ = s; } while (0)
#endif

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

int SSLClientSocketNSS::ReconnectIgnoringLastError(CompletionCallback* callback) {
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
  LeaveFunction("");
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
  LeaveFunction("");
  return rv;
}

void SSLClientSocketNSS::GetSSLInfo(SSLInfo* ssl_info) {
  EnterFunction("");
  // TODO(port): implement!
  ssl_info->Reset();
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

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL3, ssl_config_.ssl3_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL3, ssl_config_.tls1_enabled);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;

  rv = SSL_OptionSet(nss_fd_, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
  if (rv != SECSuccess)
     return ERR_UNEXPECTED;
  
  rv = SSL_BadCertHook(nss_fd_, ownBadCertHandler, NULL);
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
  int rv = SSL_ForceHandshake(nss_fd_);
  if (rv == SECSuccess) {
    // there's a callback for this, too
    completed_handshake_ = true;
    // Indicate we're ready to handle I/O.  Badly named?
    GotoState(STATE_NONE);
    LeaveFunction("");
    return OK;
  }
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    // at this point, it should have tried to send some bytes
    GotoState(STATE_HANDSHAKE_READ);
    LeaveFunction("");
    return ERR_IO_PENDING;
  }
  // TODO: map rv to net error code properly
  LeaveFunction("");
  return ERR_SSL_PROTOCOL_ERROR;
}
    
int SSLClientSocketNSS::DoPayloadRead() {
  EnterFunction(user_buf_len_);
  int rv = PR_Read(nss_fd_, user_buf_, user_buf_len_);
  if (rv >= 0) {
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
  // TODO: map rv to net error code properly
  return ERR_SSL_PROTOCOL_ERROR;
}

int SSLClientSocketNSS::DoPayloadWrite() {
  EnterFunction(user_buf_len_);
  int rv = PR_Write(nss_fd_, user_buf_, user_buf_len_);
  if (rv >= 0) {
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
  // TODO: map rv to net error code properly
  return ERR_SSL_PROTOCOL_ERROR;
}

}  // namespace net

