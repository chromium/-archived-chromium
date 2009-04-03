// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_client_socket_mac.h"

#include "base/singleton.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"

// Welcome to Mac SSL. We've been waiting for you.
//
// The Mac SSL implementation is, like the Windows and NSS implementations, a
// giant state machine. This design constraint is due to the asynchronous nature
// of our underlying transport mechanism. We can call down to read/write on the
// network, but what happens is that either it completes immediately or returns
// saying that we'll get a callback sometime in the future. In that case, we
// have to return to our caller but pick up where we left off when we
// resume. Thus the fun.
//
// On Windows, we use Security Contexts, which are driven by us. We fetch data
// from the network, we call the context to decrypt the data, and so on. On the
// Mac, however, we provide Secure Transport with callbacks to get data from the
// network, and it calls us back to fetch the data from the network for
// it. Therefore, there are different sets of states in our respective state
// machines, fewer on the Mac because Secure Transport keeps a lot of its own
// state. The discussion about what each of the states means lives in comments
// in the DoLoop() function.
//
// Secure Transport is designed for use by either blocking or non-blocking
// network I/O. If, for example, you called SSLRead() to fetch data, Secure
// Transport will, unless it has some cached data, issue a read to your network
// callback read function to fetch it some more encrypted data. It's expecting
// one of two things. If your function is hooked up to a blocking source, then
// it'll block pending receipt of the data from the other end. That's fine, as
// when you return with the data, Secure Transport will do its thing. On the
// other hand, suppose that your socket is non-blocking and tells your function
// that it would block. Then you let Secure Transport know, and it'll tell the
// original caller that it would have blocked and that they need to call it
// "later."
//
// When's "later," though? We have fully-asynchronous networking, so we get a
// callback when our data's ready. But Secure Transport has no way for us to
// tell it that data has arrived, so we must re-execute the call that triggered
// the I/O (we rely on our state machine to do this). When we do so Secure
// Transport will ask once again for the data. Chances are that it'll be the
// same request as the previous time, but that's not actually guaranteed. But as
// long as we buffer what we have and keep track of where we were, it works
// quite well.
//
// Except for network writes. They shoot this plan straight to hell.
//
// Faking a blocking connection with an asynchronous connection (theoretically
// more powerful) simply doesn't work for writing. Suppose that Secure Transport
// requests a write of data to the network. With blocking I/O, we'd just block
// until the write completed, and with non-blocking I/O we'd know how many bytes
// we wrote before we would have blocked. But with the asynchronous I/O, the
// transport underneath us can tell us that it'll let us know sometime "later"
// whether or not things succeeded, and how many bytes were written. What do we
// return to Secure Transport? We can't return a byte count, but we can't return
// "later" as we're not guaranteed to be called in the future with the same data
// to write.
//
// So, like in any good relationship, we're forced to lie. Whenever Secure
// Transport asks for data to be written, we take it all and lie about it always
// being written. We spin in a loop (see SSLWriteCallback() and
// OnWriteComplete()) independent of the main state machine writing the data to
// the network, and get the data out. The main consequence of this independence
// from the state machine is that we require a full-duplex transport underneath
// us since we can't use it to keep our reading and writing
// straight. Fortunately, the NSS implementation also has this issue to deal
// with, so we share the same Libevent-based full-duplex TCP socket.
//
// A side comment on return values might be in order. Those who haven't taken
// the time to read the documentation (ahem, header comments) in our various
// files might be a bit surprised to see result values being treated as both
// lengths and errors. Like Shimmer, they are both. In both the case of
// immediate results as well as results returned in callbacks, a negative return
// value indicates an error, a zero return value indicates end-of-stream (for
// reads), and a positive return value indicates the number of bytes read or
// written. Thus, many functions start off with |if (result < 0) return
// result;|. That gets the error condition out of the way, and from that point
// forward the result can be treated as a length.

namespace net {

namespace {

int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case errSSLWouldBlock:
      return ERR_IO_PENDING;
    case errSSLIllegalParam:
    case errSSLBadCipherSuite:
    case errSSLBadConfiguration:
      return ERR_INVALID_ARGUMENT;
    case errSSLClosedNoNotify:
      return ERR_CONNECTION_RESET;
    case errSSLConnectionRefused:
      return ERR_CONNECTION_REFUSED;
    case errSSLClosedAbort:
      return ERR_CONNECTION_ABORTED;
    case errSSLInternal:
    case errSSLCrypto:
    case errSSLFatalAlert:
    case errSSLProtocol:
      return ERR_SSL_PROTOCOL_ERROR;
    case errSSLHostNameMismatch:
      return ERR_CERT_COMMON_NAME_INVALID;
    case errSSLCertExpired:
    case errSSLCertNotYetValid:
      return ERR_CERT_DATE_INVALID;
    case errSSLNoRootCert:
    case errSSLUnknownRootCert:
      return ERR_CERT_AUTHORITY_INVALID;
    case errSSLXCertChainInvalid:
    case errSSLBadCert:
      return ERR_CERT_INVALID;
    case errSSLPeerCertRevoked:
      return ERR_CERT_REVOKED;

    case errSSLClosedGraceful:
    case noErr:
      return OK;

    case errSSLBadRecordMac:
    case errSSLBufferOverflow:
    case errSSLDecryptionFail:
    case errSSLModuleAttach:
    case errSSLNegotiation:
    case errSSLRecordOverflow:
    case errSSLSessionNotFound:
    default:
      LOG(WARNING) << "Unknown error " << status <<
          " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

OSStatus OSStatusFromNetError(int net_error) {
  switch (net_error) {
    case ERR_IO_PENDING:
      return errSSLWouldBlock;
    case ERR_INTERNET_DISCONNECTED:
    case ERR_TIMED_OUT:
    case ERR_CONNECTION_ABORTED:
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_REFUSED:
    case ERR_ADDRESS_UNREACHABLE:
    case ERR_ADDRESS_INVALID:
      return errSSLClosedAbort;
    case OK:
      return noErr;
    default:
      LOG(WARNING) << "Unknown error " << net_error <<
          " mapped to errSSLIllegalParam";
      return errSSLIllegalParam;
  }
}

// Converts from a cipher suite to its key size. If the suite is marked with a
// **, it's not actually implemented in Secure Transport and won't be returned
// (but we'll code for it anyway).  The reference here is
// http://www.opensource.apple.com/darwinsource/10.5.5/libsecurity_ssl-32463/lib/cipherSpecs.c
// Seriously, though, there has to be an API for this, but I can't find one.
// Anybody?
int KeySizeOfCipherSuite(SSLCipherSuite suite) {
  switch (suite) {
    // SSL 2 only

    case SSL_RSA_WITH_DES_CBC_MD5:
      return 56;
    case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
      return 112;
    case SSL_RSA_WITH_RC2_CBC_MD5:
    case SSL_RSA_WITH_IDEA_CBC_MD5:              // **
      return 128;
    case SSL_NO_SUCH_CIPHERSUITE:                // **
      return 0;

    // SSL 2, 3, TLS

    case SSL_NULL_WITH_NULL_NULL:
    case SSL_RSA_WITH_NULL_MD5:
    case SSL_RSA_WITH_NULL_SHA:                  // **
    case SSL_FORTEZZA_DMS_WITH_NULL_SHA:         // **
      return 0;
    case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
    case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
    case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:   // **
    case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:   // **
    case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
    case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
      return 40;
    case SSL_RSA_WITH_DES_CBC_SHA:
    case SSL_DH_DSS_WITH_DES_CBC_SHA:            // **
    case SSL_DH_RSA_WITH_DES_CBC_SHA:            // **
    case SSL_DHE_DSS_WITH_DES_CBC_SHA:
    case SSL_DHE_RSA_WITH_DES_CBC_SHA:
    case SSL_DH_anon_WITH_DES_CBC_SHA:
      return 56;
    case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA: // **
      return 80;
    case SSL_RSA_WITH_3DES_EDE_CBC_SHA:
    case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:       // **
    case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:       // **
    case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
    case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case SSL_DH_anon_WITH_3DES_EDE_CBC_SHA:
      return 112;
    case SSL_RSA_WITH_RC4_128_MD5:
    case SSL_RSA_WITH_RC4_128_SHA:
    case SSL_RSA_WITH_IDEA_CBC_SHA:              // **
    case SSL_DH_anon_WITH_RC4_128_MD5:
      return 128;

    // TLS AES options (see RFC 3268)

    case TLS_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_anon_WITH_AES_128_CBC_SHA:
      return 128;
    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_anon_WITH_AES_256_CBC_SHA:
      return 256;

    default:
      return -1;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

SSLClientSocketMac::SSLClientSocketMac(ClientSocket* transport_socket,
                                       const std::string& hostname,
                                       const SSLConfig& ssl_config)
    : io_callback_(this, &SSLClientSocketMac::OnIOComplete),
      write_callback_(this, &SSLClientSocketMac::OnWriteComplete),
      transport_(transport_socket),
      hostname_(hostname),
      ssl_config_(ssl_config),
      user_callback_(NULL),
      next_state_(STATE_NONE),
      next_io_state_(STATE_NONE),
      server_cert_status_(0),
      completed_handshake_(false),
      ssl_context_(NULL),
      pending_send_error_(OK),
      recv_buffer_head_slop_(0),
      recv_buffer_tail_slop_(0) {
}

SSLClientSocketMac::~SSLClientSocketMac() {
  Disconnect();
}

int SSLClientSocketMac::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(next_state_ == STATE_NONE);
  DCHECK(!user_callback_);

  OSStatus status = noErr;

  status = SSLNewContext(false, &ssl_context_);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetProtocolVersionEnabled(ssl_context_,
                                        kSSLProtocol2,
                                        ssl_config_.ssl2_enabled);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetProtocolVersionEnabled(ssl_context_,
                                        kSSLProtocol3,
                                        ssl_config_.ssl3_enabled);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetProtocolVersionEnabled(ssl_context_,
                                        kTLSProtocol1,
                                        ssl_config_.tls1_enabled);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetIOFuncs(ssl_context_, SSLReadCallback, SSLWriteCallback);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetConnection(ssl_context_, this);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetPeerDomainName(ssl_context_, hostname_.c_str(),
                                hostname_.length());
  if (status)
    return NetErrorFromOSStatus(status);

  next_state_ = STATE_HANDSHAKE;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

void SSLClientSocketMac::Disconnect() {
  completed_handshake_ = false;

  if (ssl_context_) {
    SSLClose(ssl_context_);
    SSLDisposeContext(ssl_context_);
    ssl_context_ = NULL;
  }

  transport_->Disconnect();
}

bool SSLClientSocketMac::IsConnected() const {
  // Ideally, we should also check if we have received the close_notify alert
  // message from the server, and return false in that case.  We're not doing
  // that, so this function may return a false positive.  Since the upper
  // layer (HttpNetworkTransaction) needs to handle a persistent connection
  // closed by the server when we send a request anyway, a false positive in
  // exchange for simpler code is a good trade-off.
  return completed_handshake_ && transport_->IsConnected();
}

bool SSLClientSocketMac::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so transport_->IsConnectedAndIdle()
  // returns the desired false when we receive close_notify.
  return completed_handshake_ && transport_->IsConnectedAndIdle();
}

int SSLClientSocketMac::Read(char* buf, int buf_len,
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

int SSLClientSocketMac::Write(const char* buf, int buf_len,
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

void SSLClientSocketMac::GetSSLInfo(SSLInfo* ssl_info) {
  DCHECK(completed_handshake_);
  OSStatus status;

  ssl_info->Reset();

  // set cert
  CFArrayRef certs;
  status = SSLCopyPeerCertificates(ssl_context_, &certs);
  if (!status) {
    DCHECK(CFArrayGetCount(certs) > 0);

    SecCertificateRef client_cert =
        static_cast<SecCertificateRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(certs, 0)));
    CFRetain(client_cert);
    ssl_info->cert = X509Certificate::CreateFromHandle(
        client_cert, X509Certificate::SOURCE_FROM_NETWORK);
    CFRelease(certs);
  }

  // update status
  ssl_info->cert_status = server_cert_status_;

  // security info
  SSLCipherSuite suite;
  status = SSLGetNegotiatedCipher(ssl_context_, &suite);
  if (!status)
    ssl_info->security_bits = KeySizeOfCipherSuite(suite);
}

void SSLClientSocketMac::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void SSLClientSocketMac::OnIOComplete(int result) {
  if (next_io_state_ != STATE_NONE) {
    State next_state = next_state_;
    next_state_ = next_io_state_;
    next_io_state_ = STATE_NONE;
    result = DoLoop(result);
    next_state_ = next_state;
  }
  if (next_state_ != STATE_NONE) {
    int rv = DoLoop(result);
    if (rv != ERR_IO_PENDING)
      DoCallback(rv);
  }
}

// This is the main loop driving the state machine. Most calls coming from the
// outside just set up a few variables and jump into here.
int SSLClientSocketMac::DoLoop(int last_io_result) {
  DCHECK(next_state_ != STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_HANDSHAKE:
        // Do the SSL/TLS handshake.
        rv = DoHandshake();
        break;
      case STATE_READ_COMPLETE:
        // A read off the network is complete; do the paperwork.
        rv = DoReadComplete(rv);
        break;
      case STATE_PAYLOAD_READ:
        // Do a read of data from the network.
        rv = DoPayloadRead();
        break;
      case STATE_PAYLOAD_WRITE:
        // Do a write of data to the network.
        rv = DoPayloadWrite();
        break;
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state";
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketMac::DoHandshake() {
  OSStatus status = SSLHandshake(ssl_context_);

  if (status == errSSLWouldBlock)
    next_state_ = STATE_HANDSHAKE;

  if (status == noErr)
    completed_handshake_ = true;

  int net_error = NetErrorFromOSStatus(status);

  // At this point we have a connection. For now, we're going to use the default
  // certificate verification that the system does, and accept its answer for
  // the cert status. In the future, we'll need to call SSLSetEnableCertVerify
  // to disable cert verification and do the verification ourselves. This allows
  // very fine-grained control over what we'll accept for certification.
  // TODO(avi): ditto

  // TODO(wtc): for now, always check revocation.
  server_cert_status_ = CERT_STATUS_REV_CHECKING_ENABLED;
  if (net_error)
    server_cert_status_ |= MapNetErrorToCertStatus(net_error);

  return net_error;
}

int SSLClientSocketMac::DoReadComplete(int result) {
  if (result < 0)
    return result;

  recv_buffer_tail_slop_ -= result;

  return result;
}

void SSLClientSocketMac::OnWriteComplete(int result) {
  if (result < 0) {
    pending_send_error_ = result;
    return;
  }

  send_buffer_.erase(send_buffer_.begin(),
                     send_buffer_.begin() + result);

  if (!send_buffer_.empty())
    SSLWriteCallback(this, NULL, NULL);
}

int SSLClientSocketMac::DoPayloadRead() {
  size_t processed;
  OSStatus status = SSLRead(ssl_context_,
                            user_buf_,
                            user_buf_len_,
                            &processed);

  // There's a subtle difference here in semantics of the "would block" errors.
  // In our code, ERR_IO_PENDING means the whole operation is async, while
  // errSSLWouldBlock means that the stream isn't ending (and is often returned
  // along with partial data). So even though "would block" is returned, if we
  // have data, let's just return it.

  if (processed > 0) {
    next_state_ = STATE_NONE;
    return processed;
  }

  if (status == errSSLWouldBlock) {
    next_state_ = STATE_PAYLOAD_READ;
  }

  return NetErrorFromOSStatus(status);
}

int SSLClientSocketMac::DoPayloadWrite() {
  size_t processed;
  OSStatus status = SSLWrite(ssl_context_,
                             user_buf_,
                             user_buf_len_,
                             &processed);

  if (processed > 0)
    return processed;

  return NetErrorFromOSStatus(status);
}

// Handling the reading from the network is one of those things that should be
// simpler than it is. Ideally, we'd have some kind of ring buffer. For now, a
// std::vector<char> will have to do.
//
// The need for a buffer at all comes from the difference between an
// asynchronous connection (which is what we have) and a non-blocking connection
// (which is what we fake for Secure Transport). When Secure Transport calls us
// to read data, we call our underlying transport, which will likely tell us
// that it'll do a callback. When that happens, we need to tell Secure Transport
// that we've "blocked". When the callback happens, we have a chunk of data that
// we need to feed to Secure Transport, but it's not interested. It'll ask for
// it again when we call it again, so we need to hold on to the data.
//
// Why keep our own buffer? Well, when we execute a read and the underlying
// transport says that it'll do a callback, it keeps the pointer to the
// buffer. We can't pass it the buffer that Secure Transport gave us to fill, as
// we can't guarantee its lifetime.
//
// The basic idea, then, is this: we have a buffer filled with the data that
// we've read from the network but haven't given to Secure Transport
// yet. Whenever we read from the network the first thing we do is ensure we
// have enough room in the buffer for the read. We enlarge the buffer to be big
// enough to hold both our existing data and the new data, and then we mark the
// extra space at the end as "tail slop." Slop is just space at the ends of the
// buffer that's going to be used for data but isn't (yet). A diagram:
//
// +--------------------------------------+--------------------------------+
// | existing good data ~~~~~~~~~~~~~~~~~ | tail slop area ~~~~~~~~~~~~~~~ |
// +--------------------------------------+--------------------------------+
//
// When executing a read, we pass a pointer to the beginning of the tail slop
// area (guaranteed to be contiguous space because it's a vector, unlike, say, a
// deque (sigh)) and the size of the tail slop. When we get data (either here in
// SSLReadCallback() or above in DoReadComplete()) we subtract the number of
// bytes received from the tail slop value. That moves those bytes
// (conceptually, not physically) from the tail slop area to the area containing
// real data.
//
// The idea is still pretty simple. We enlarge the tail slop, call our
// underlying network, get data, shrink the slop area to match, copy requested
// data back into our caller's buffer, and delete the data from the head of the
// vector.
//
// Except for a nasty little problem. Asynchronous I/O calls keep the buffer
// pointer.
//
// This leads to the following scenario: we have a few bytes of good data in our
// buffer. But our caller requests more than that. We oblige by enlarging the
// tail slop, and calling our underlying provider, but the provider says that
// it'll call us back later. So we shrug our shoulders, copy what we do have
// into our caller's buffer and...
//
// Wait. We can't delete the data from the head of our vector. That would
// invalidate the pointer that we just gave to our provider. So instead, in that
// case we keep track of where the good data starts by keeping a "head slop"
// value, which just notes what data we've already sent and that is useless to
// us but that we can't delete because we have I/O in flight depending on us
// leaving the buffer alone.
//
// I hear what you're saying. "We need to use a ring buffer!" You write it,
// then, and I'll use it. Here are the features it needs. First, it needs to be
// able to have contiguous segments of arbitrary length attached to it to create
// read buffers. Second, each of those segments must have a "used" length
// indicator, so if it was half-filled by a previous data read, but the next
// data read is for more than there's space left, a new segment can be created
// for the new read without leaving an internal gap.
//
// Get to it.
//
// (sigh) Who am I kidding? TODO(avi): write the aforementioned ring buffer

// static
OSStatus SSLClientSocketMac::SSLReadCallback(SSLConnectionRef connection,
                                             void* data,
                                             size_t* data_length) {
  DCHECK(data);
  DCHECK(data_length);
  SSLClientSocketMac* us =
      const_cast<SSLClientSocketMac*>(
          static_cast<const SSLClientSocketMac*>(connection));

  // If we have I/O in flight, promise we'll get back to them and use the
  // existing callback to do so

  if (us->next_io_state_ == STATE_READ_COMPLETE) {
    *data_length = 0;
    return errSSLWouldBlock;
  }

  // Start with what's in the buffer

  size_t total_read = us->recv_buffer_.size() - us->recv_buffer_head_slop_ -
                      us->recv_buffer_tail_slop_;

  // Resize the buffer if needed

  if (us->recv_buffer_.size() - us->recv_buffer_head_slop_ < *data_length) {
    us->recv_buffer_.resize(us->recv_buffer_head_slop_ + *data_length);
    us->recv_buffer_tail_slop_ = *data_length - total_read;
  }

  int rv = 1;  // any old value to spin the loop below
  while (rv > 0 && total_read < *data_length) {
    rv = us->transport_->Read(&us->recv_buffer_[us->recv_buffer_head_slop_ +
                                                total_read],
                              us->recv_buffer_tail_slop_,
                              &us->io_callback_);

    if (rv > 0) {
      total_read += rv;
      us->recv_buffer_tail_slop_ -= rv;
    }
  }

  *data_length = total_read;
  if (total_read) {
    memcpy(data, &us->recv_buffer_[us->recv_buffer_head_slop_], total_read);
    if (rv == ERR_IO_PENDING) {
      // We have I/O in flight which is going to land in our buffer. We can't
      // shuffle things around, so we need to just fiddle with pointers.
      us->recv_buffer_head_slop_ += total_read;
    } else {
      us->recv_buffer_.erase(us->recv_buffer_.begin(),
                             us->recv_buffer_.begin() +
                             total_read +
                             us->recv_buffer_head_slop_);
      us->recv_buffer_head_slop_ = 0;
    }
  }

  if (rv == ERR_IO_PENDING) {
    us->next_io_state_ = STATE_READ_COMPLETE;
  }

  if (rv < 0)
    return OSStatusFromNetError(rv);

  return noErr;
}

// static
OSStatus SSLClientSocketMac::SSLWriteCallback(SSLConnectionRef connection,
                                              const void* data,
                                              size_t* data_length) {
  SSLClientSocketMac* us =
      const_cast<SSLClientSocketMac*>(
          static_cast<const SSLClientSocketMac*>(connection));

  if (us->pending_send_error_ != OK) {
    OSStatus status = OSStatusFromNetError(us->pending_send_error_);
    us->pending_send_error_ = OK;
    return status;
  }

  if (data)
    us->send_buffer_.insert(us->send_buffer_.end(),
                            static_cast<const char*>(data),
                            static_cast<const char*>(data) + *data_length);
  int rv;
  do {
    rv = us->transport_->Write(&us->send_buffer_[0],
                               us->send_buffer_.size(),
                               &us->write_callback_);
    if (rv > 0) {
      us->send_buffer_.erase(us->send_buffer_.begin(),
                             us->send_buffer_.begin() + rv);

    }
  } while (rv > 0 && !us->send_buffer_.empty());

  if (rv < 0 && rv != ERR_IO_PENDING) {
    return OSStatusFromNetError(rv);
  }

  // always lie to our caller
  return noErr;
}

}  // namespace net
