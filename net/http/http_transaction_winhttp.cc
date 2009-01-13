// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_transaction_winhttp.h"

#include <winhttp.h>

#include "base/lock.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth_cache.h"
#include "net/base/cert_status_flags.h"
#include "net/base/dns_resolution_observer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/ssl_config_service.h"
#include "net/base/upload_data_stream.h"
#include "net/http/cert_status_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/winhttp_request_throttle.h"
#include "net/proxy/proxy_resolver_winhttp.h"

#pragma comment(lib, "winhttp.lib")
#pragma warning(disable: 4355)

using base::Time;

namespace net {

static int TranslateOSError(DWORD error) {
  switch (error) {
    case ERROR_SUCCESS:
      return OK;
    case ERROR_FILE_NOT_FOUND:
      return ERR_FILE_NOT_FOUND;
    case ERROR_HANDLE_EOF:  // TODO(wtc): return OK?
      return ERR_CONNECTION_CLOSED;
    case ERROR_INVALID_HANDLE:
      return ERR_INVALID_HANDLE;
    case ERROR_INVALID_PARAMETER:
      return ERR_INVALID_ARGUMENT;

    case ERROR_WINHTTP_CANNOT_CONNECT:
      return ERR_CONNECTION_FAILED;
    case ERROR_WINHTTP_TIMEOUT:
      return ERR_TIMED_OUT;
    case ERROR_WINHTTP_INVALID_URL:
      return ERR_INVALID_URL;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
      return ERR_NAME_NOT_RESOLVED;
    case ERROR_WINHTTP_OPERATION_CANCELLED:
      return ERR_ABORTED;
    case ERROR_WINHTTP_SECURE_CHANNEL_ERROR:
    case ERROR_WINHTTP_SECURE_FAILURE:
    case SEC_E_ILLEGAL_MESSAGE:
      return ERR_SSL_PROTOCOL_ERROR;
    case SEC_E_ALGORITHM_MISMATCH:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED:
      return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:
      return ERR_UNKNOWN_URL_SCHEME;
    case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
      return ERR_INVALID_RESPONSE;

    // SSL certificate errors
    case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:
      return ERR_CERT_COMMON_NAME_INVALID;
    case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:
      return ERR_CERT_DATE_INVALID;
    case ERROR_WINHTTP_SECURE_INVALID_CA:
      return ERR_CERT_AUTHORITY_INVALID;
    case ERROR_WINHTTP_SECURE_CERT_REV_FAILED:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case ERROR_WINHTTP_SECURE_CERT_REVOKED:
      return ERR_CERT_REVOKED;
    case ERROR_WINHTTP_SECURE_INVALID_CERT:
      return ERR_CERT_INVALID;

    default:
      DCHECK(error != ERROR_IO_PENDING);  // WinHTTP doesn't use this error.
      LOG(WARNING) << "Unknown error " << error
                   << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

static int TranslateLastOSError() {
  return TranslateOSError(GetLastError());
}

// Clear certificate errors that we want to ignore.
static DWORD FilterSecureFailure(DWORD status, int load_flags) {
  if (load_flags & LOAD_IGNORE_CERT_COMMON_NAME_INVALID)
    status &= ~WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID;
  if (load_flags & LOAD_IGNORE_CERT_DATE_INVALID)
    status &= ~WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID;
  if (load_flags & LOAD_IGNORE_CERT_AUTHORITY_INVALID)
    status &= ~WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA;
  if (load_flags & LOAD_IGNORE_CERT_WRONG_USAGE)
    status &= ~WINHTTP_CALLBACK_STATUS_FLAG_CERT_WRONG_USAGE;
  return status;
}

static DWORD MapSecureFailureToError(DWORD status) {
  // A certificate may have multiple errors.  We report the most
  // serious error.

  // Unrecoverable errors
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR)
    return ERROR_WINHTTP_SECURE_CHANNEL_ERROR;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT)
    return ERROR_WINHTTP_SECURE_INVALID_CERT;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED)
    return ERROR_WINHTTP_SECURE_CERT_REVOKED;

  // Recoverable errors
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA)
    return ERROR_WINHTTP_SECURE_INVALID_CA;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID)
    return ERROR_WINHTTP_SECURE_CERT_CN_INVALID;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID)
    return ERROR_WINHTTP_SECURE_CERT_DATE_INVALID;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_WRONG_USAGE)
    return ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE;

  // Unknown status.  Give it the benefit of the doubt.
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED)
    return ERROR_WINHTTP_SECURE_CERT_REV_FAILED;

  // Map a status of 0 to the generic secure failure error.  We have seen a
  // case where WinHttp doesn't notify us of a secure failure (so status is 0)
  // before notifying us of a request error with ERROR_WINHTTP_SECURE_FAILURE.
  // (WinInet fails with ERROR_INTERNET_SECURITY_CHANNEL_ERROR in that case.)
  return ERROR_WINHTTP_SECURE_FAILURE;
}

static int MapSecureFailureToCertStatus(DWORD status) {
  int cert_status = 0;

  if (status & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT)
    cert_status |= CERT_STATUS_INVALID;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED)
    cert_status |= CERT_STATUS_REVOKED;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA)
    cert_status |= CERT_STATUS_AUTHORITY_INVALID;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID)
    cert_status |= CERT_STATUS_COMMON_NAME_INVALID;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID)
    cert_status |= CERT_STATUS_DATE_INVALID;
  if (status & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED)
    cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  return cert_status;
  // TODO(jcampan): what about ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE?
}

// Session --------------------------------------------------------------------

class HttpTransactionWinHttp::Session
  : public base::RefCounted<HttpTransactionWinHttp::Session> {
 public:
  enum {
    // By default WinHTTP enables only SSL3 and TLS1.
    SECURE_PROTOCOLS_SSL3_TLS1 = WINHTTP_FLAG_SECURE_PROTOCOL_SSL3 |
                                 WINHTTP_FLAG_SECURE_PROTOCOL_TLS1
  };

  explicit Session(ProxyService* proxy_service);

  // Opens the primary WinHttp session handle.
  bool Init(const std::string& user_agent);

  // Opens the alternative WinHttp session handle for TLS-intolerant servers.
  bool InitNoTLS(const std::string& user_agent);

  void AddRefBySessionCallback();

  void ReleaseBySessionCallback();

  // The primary WinHttp session handle.
  HINTERNET internet() { return internet_; }

  // An alternative WinHttp session handle.  It is not opened until we have
  // encountered a TLS-intolerant server and used for those servers only.
  // TLS is disabled in this session.
  HINTERNET internet_no_tls() { return internet_no_tls_; }

  // The message loop of the thread where the session was created.
  MessageLoop* message_loop() { return message_loop_; }

  ProxyService* proxy_service() { return proxy_service_; }

  // Gets the HTTP authentication cache for the session.
  AuthCache* auth_cache() { return &auth_cache_; }

  HANDLE handle_closing_event() const { return handle_closing_event_; }

  CertStatusCache* cert_status_cache() { return &cert_status_cache_; }

  bool rev_checking_enabled() const { return rev_checking_enabled_; }

  bool tls_enabled() const {
    return (secure_protocols_ & WINHTTP_FLAG_SECURE_PROTOCOL_TLS1) != 0;
  }

  bool ShouldIgnoreCertRev(const std::string& origin) const {
    OriginSet::const_iterator pos = ignore_cert_rev_servers_.find(origin);
    return pos != ignore_cert_rev_servers_.end();
  }

  void IgnoreCertRev(const std::string& origin) {
    ignore_cert_rev_servers_.insert(origin);
  }

  WinHttpRequestThrottle* request_throttle() {
    return &request_throttle_;
  }

 private:
  friend class base::RefCounted<HttpTransactionWinHttp::Session>;

  ~Session();

  // Called by the destructor only.
  void WaitUntilCallbacksAllDone();

  HINTERNET OpenWinHttpSession(const std::string& user_agent);

  // Get the SSL configuration settings and save them in rev_checking_enabled_
  // and secure_protocols_.
  void GetSSLConfig();

  HINTERNET internet_;
  HINTERNET internet_no_tls_;
  MessageLoop* message_loop_;
  ProxyService* proxy_service_;
  AuthCache auth_cache_;

  // This event object is used when destroying a transaction.  It is given
  // to the transaction's session callback if WinHTTP still has the caller's
  // data (request info or read buffer) and we need to wait until WinHTTP is
  // done with the data.
  HANDLE handle_closing_event_;

  // The following members ensure a clean destruction of the Session object.
  // The Session destructor waits until all the request handles have been
  // terminated by WinHTTP, at which point no more status callbacks will
  // reference the MessageLoop of the Session.
  //
  // quit_event_ is the event object used for this wait.
  //
  // lock_ protects session_callback_ref_count_ and quitting_.
  //
  // session_callback_ref_count_ is the number of SessionCallback objects
  // that may reference the MessageLoop of the Session.
  //
  // The boolean quitting_ is true when the Session object is being
  // destructed.
  HANDLE quit_event_;
  Lock lock_;
  int session_callback_ref_count_;
  bool quitting_;

  // We use a cache to store the certificate error as we cannot always rely on
  // WinHTTP to provide us the SSL error once we restarted a connection asking
  // to ignored errors.
  CertStatusCache cert_status_cache_;

  // SSL settings
  bool rev_checking_enabled_;
  DWORD secure_protocols_;

  // The servers for which certificate revocation should be ignored.
  //
  // WinHTTP verifies each certificate only once and caches the certificate
  // verification results, so if we ever ignore certificate revocation for a
  // server, we cannot enable revocation checking again for that server for
  // the rest of the session.
  //
  // If we honor changes to the rev_checking_enabled system setting during
  // the session, we will have to remember all the servers we have visited
  // while the rev_checking_enabled setting is false.  This will consume a
  // lot of memory.  So we now require the users to restart Chrome for a
  // rev_checking_enabled change to take effect, just like IE does.
  typedef std::set<std::string> OriginSet;
  OriginSet ignore_cert_rev_servers_;

  WinHttpRequestThrottle request_throttle_;
};

HttpTransactionWinHttp::Session::Session(ProxyService* proxy_service)
    : internet_(NULL),
      internet_no_tls_(NULL),
      proxy_service_(proxy_service),
      session_callback_ref_count_(0),
      quitting_(false) {
  GetSSLConfig();

  // Save the current message loop for callback notifications.
  message_loop_ = MessageLoop::current();

  handle_closing_event_ = CreateEvent(NULL,
                                      FALSE,  // auto-reset
                                      FALSE,  // initially nonsignaled
                                      NULL);  // unnamed

  quit_event_ = CreateEvent(NULL,
                            FALSE,  // auto-reset
                            FALSE,  // initially nonsignaled
                            NULL);  // unnamed
}

HttpTransactionWinHttp::Session::~Session() {
  if (internet_) {
    WinHttpCloseHandle(internet_);
    if (internet_no_tls_)
      WinHttpCloseHandle(internet_no_tls_);

    // Ensure that all status callbacks that may reference the MessageLoop
    // of this thread are done before we can allow the current thread to exit.
    WaitUntilCallbacksAllDone();
  }

  if (handle_closing_event_)
    CloseHandle(handle_closing_event_);
  if (quit_event_)
    CloseHandle(quit_event_);
}

bool HttpTransactionWinHttp::Session::Init(const std::string& user_agent) {
  DCHECK(!internet_);

  internet_ = OpenWinHttpSession(user_agent);

  if (!internet_)
    return false;

  if (secure_protocols_ != SECURE_PROTOCOLS_SSL3_TLS1) {
    BOOL rv = WinHttpSetOption(internet_, WINHTTP_OPTION_SECURE_PROTOCOLS,
                               &secure_protocols_, sizeof(secure_protocols_));
    DCHECK(rv);
  }

  return true;
}

bool HttpTransactionWinHttp::Session::InitNoTLS(
    const std::string& user_agent) {
  DCHECK(tls_enabled());
  DCHECK(internet_);
  DCHECK(!internet_no_tls_);

  internet_no_tls_ = OpenWinHttpSession(user_agent);

  if (!internet_no_tls_)
    return false;

  DWORD protocols = secure_protocols_ & ~WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
  BOOL rv = WinHttpSetOption(internet_no_tls_,
                             WINHTTP_OPTION_SECURE_PROTOCOLS,
                             &protocols, sizeof(protocols));
  DCHECK(rv);

  return true;
}

void HttpTransactionWinHttp::Session::AddRefBySessionCallback() {
  AutoLock lock(lock_);
  session_callback_ref_count_++;
}

void HttpTransactionWinHttp::Session::ReleaseBySessionCallback() {
  bool need_to_signal;
  {
    AutoLock lock(lock_);
    session_callback_ref_count_--;
    need_to_signal = (quitting_ && session_callback_ref_count_ == 0);
  }
  if (need_to_signal)
    SetEvent(quit_event_);
}

// This is called by the Session destructor only.  By now the transaction
// factory and all the transactions have been destructed.  This means that
// new transactions can't be created, and existing transactions can't be
// started, which in turn implies that session_callback_ref_count_ cannot
// increase.  We wait until session_callback_ref_count_ drops to 0.
void HttpTransactionWinHttp::Session::WaitUntilCallbacksAllDone() {
  bool need_to_wait;
  {
    AutoLock lock(lock_);
    quitting_ = true;
    need_to_wait = (session_callback_ref_count_ != 0);
  }
  if (need_to_wait)
    WaitForSingleObject(quit_event_, INFINITE);
  DCHECK(session_callback_ref_count_ == 0);
}

HINTERNET HttpTransactionWinHttp::Session::OpenWinHttpSession(
    const std::string& user_agent) {
  // Proxy config will be set explicitly for each request.
  //
  // Although UA string will also be set explicitly for each request, HTTP
  // CONNECT requests use the UA string of the session handle, so we have to
  // pass a UA string to WinHttpOpen.
  HINTERNET internet = WinHttpOpen(ASCIIToWide(user_agent).c_str(),
                                   WINHTTP_ACCESS_TYPE_NO_PROXY,
                                   WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS,
                                   WINHTTP_FLAG_ASYNC);
  if (!internet)
    return internet;

  // Use a 90-second timeout (1.5 times the default) for connect.  Disable
  // name resolution, send, and receive timeouts.  We expect our consumer to
  // apply timeouts or provide controls for users to stop requests that are
  // taking too long.
  BOOL rv = WinHttpSetTimeouts(internet, 0, 90000, 0, 0);
  DCHECK(rv);

  return internet;
}

void HttpTransactionWinHttp::Session::GetSSLConfig() {
  SSLConfig ssl_config;
  SSLConfigService::GetSSLConfigNow(&ssl_config);
  rev_checking_enabled_ = ssl_config.rev_checking_enabled;
  secure_protocols_ = 0;
  if (ssl_config.ssl2_enabled)
    secure_protocols_ |= WINHTTP_FLAG_SECURE_PROTOCOL_SSL2;
  if (ssl_config.ssl3_enabled)
    secure_protocols_ |= WINHTTP_FLAG_SECURE_PROTOCOL_SSL3;
  if (ssl_config.tls1_enabled)
    secure_protocols_ |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
}

// SessionCallback ------------------------------------------------------------

class HttpTransactionWinHttp::SessionCallback
  : public base::RefCountedThreadSafe<HttpTransactionWinHttp::SessionCallback> {
 public:
  SessionCallback(HttpTransactionWinHttp* trans, Session* session)
      : trans_(trans),
        session_(session),
        load_state_(LOAD_STATE_IDLE),
        handle_closing_event_(NULL),
        bytes_available_(0),
        read_buf_(NULL),
        read_buf_len_(0),
        secure_failure_(0),
        connection_was_opened_(false),
        request_was_probably_sent_(false),
        response_was_received_(false),
        response_is_empty_(true) {
  }

  // Called when the associated trans_ has to reopen its connection and
  // request handles to recover from certain SSL errors.  Resets the members
  // that may have been modified at that point.
  void ResetForNewRequest() {
    secure_failure_ = 0;
    connection_was_opened_ = false;
  }

  void DropTransaction() {
    trans_ = NULL;
  }

  void Notify(DWORD status, DWORD_PTR result, DWORD error) {
    DWORD secure_failure = 0;
    if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
      switch (error) {
        // WinHttp sends this error code in two interesting cases: 1) when a
        // response header is malformed, and 2) when a response is empty.  In
        // the latter case, we want to actually resend the request if the
        // request was sent over a reused "keep-alive" connection.  This is a
        // risky thing to do since it is possible that the server did receive
        // our request, but it is unfortunately required to support HTTP keep-
        // alive connections properly, and other browsers all do this too.
        case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
          if (empty_response_was_received() && !connection_was_opened_)
            error = ERROR_WINHTTP_RESEND_REQUEST;
          break;
        case ERROR_WINHTTP_SECURE_FAILURE:
          secure_failure = secure_failure_;
          break;
      }
    } else if (status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE) {
      secure_failure = secure_failure_;
    }
    session_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &HttpTransactionWinHttp::SessionCallback::OnNotify,
                          status, result, error, secure_failure));
  }

  // Calls WinHttpReadData and returns its return value.
  BOOL ReadData(HINTERNET request_handle);

  void OnHandleClosing() {
    if (handle_closing_event_)
      SetEvent(handle_closing_event_);
    session_->ReleaseBySessionCallback();
    Release();
  }

  // Modified from any thread.
  void set_load_state(LoadState state) {
    load_state_ = state;
  }
  LoadState load_state() const {
    return load_state_;
  }

  int bytes_available() const { return bytes_available_; }
  void set_bytes_available(int n) { bytes_available_ = n; }
  void reduce_bytes_available(int n) { bytes_available_ -= n; }

  char* read_buf() const { return read_buf_; }
  void set_read_buf(char* buf) { read_buf_ = buf; }

  int read_buf_len() const { return read_buf_len_; }
  void set_read_buf_len(int buf_len) { read_buf_len_ = buf_len; }

  // Tells this SessionCallback to signal this event when receiving the
  // handle closing status callback.
  void set_handle_closing_event(HANDLE event) {
    handle_closing_event_ = event;
  }

  void set_secure_failure(DWORD flags) { secure_failure_ = flags; }

  void did_open_connection() {
    connection_was_opened_ = true;
  }

  void did_start_sending_request() {
    request_was_probably_sent_ = true;
  }
  bool request_was_probably_sent() const {
    return request_was_probably_sent_;
  }

  void did_receive_bytes(DWORD count) {
    response_was_received_ = true;
    if (count)
      response_is_empty_ = false;
  }

 private:
  friend base::RefCountedThreadSafe<HttpTransactionWinHttp::SessionCallback>;
  ~SessionCallback() {}

  void OnNotify(DWORD status,
                DWORD_PTR result,
                DWORD error,
                DWORD secure_failure) {
    if (trans_)
      trans_->HandleStatusCallback(status, result, error, secure_failure);

    // Balances the AddRefs made by the transaction object after an async
    // WinHTTP call.
    Release();
  }

  bool empty_response_was_received() const {
    return response_was_received_ && response_is_empty_;
  }

  HttpTransactionWinHttp* trans_;

  // Session is reference-counted, but this is a plain pointer.  The
  // reference on the Session owned by SessionCallback is managed using
  // Session::AddRefBySessionCallback and Session::ReleaseBySessionCallback.
  Session* session_;

  // Modified from any thread.
  volatile LoadState load_state_;

  // Amount of data available reported by WinHttpQueryDataAvailable that
  // haven't been consumed by WinHttpReadData.
  int bytes_available_;

  // Caller's read buffer and buffer size, to be passed to WinHttpReadData.
  // These are used by the IO thread and the thread WinHTTP uses to make
  // status callbacks, but not at the same time.
  char* read_buf_;
  int read_buf_len_;

  // If not null, we set this event on receiving the handle closing callback.
  HANDLE handle_closing_event_;

  // The secure connection failure flags reported by the
  // WINHTTP_CALLBACK_STATUS_SECURE_FAILURE status callback.
  DWORD secure_failure_;

  // True if a connection was opened for this request.
  bool connection_was_opened_;

  // True if the request may have been sent to the server (and therefore we
  // should not restart the request).
  bool request_was_probably_sent_;

  // True if any response was received.
  bool response_was_received_;

  // True if we have an empty response (no headers, no status line, nothing).
  bool response_is_empty_;
};

BOOL HttpTransactionWinHttp::SessionCallback::ReadData(
    HINTERNET request_handle) {
  DCHECK(bytes_available_ >= 0);
  char* buf = read_buf_;
  read_buf_ = NULL;
  int bytes_to_read = std::min(bytes_available_, read_buf_len_);
  read_buf_len_ = 0;
  if (!bytes_to_read)
    bytes_to_read = 1;

  // Because of how WinHTTP fills memory when used asynchronously, Purify isn't
  // able to detect that it's been initialized, so it scans for 0xcd in the
  // buffer and reports UMRs (uninitialized memory reads) for those individual
  // bytes. We override that to avoid the false error reports.
  // See http://b/issue?id=1173916.
  base::MemoryDebug::MarkAsInitialized(buf, bytes_to_read);
  return WinHttpReadData(request_handle, buf, bytes_to_read, NULL);
}

// static
void HttpTransactionWinHttp::StatusCallback(HINTERNET handle,
                                            DWORD_PTR context,
                                            DWORD status,
                                            LPVOID status_info,
                                            DWORD status_info_len) {
  SessionCallback* callback = reinterpret_cast<SessionCallback*>(context);

  switch (status) {
    case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
      if (callback)
        callback->OnHandleClosing();
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
      callback->set_load_state(LOAD_STATE_CONNECTING);
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
      callback->did_open_connection();
      break;
    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
      callback->set_load_state(LOAD_STATE_SENDING_REQUEST);
      callback->did_start_sending_request();
      break;
    case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
      callback->set_load_state(LOAD_STATE_WAITING_FOR_RESPONSE);
      break;
    case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
      callback->did_receive_bytes(*static_cast<DWORD*>(status_info));
      break;
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
      DCHECK(callback->bytes_available() == 0);
      DCHECK(status_info_len == sizeof(DWORD));
      callback->set_bytes_available(static_cast<DWORD*>(status_info)[0]);
      if (!callback->ReadData(handle))
        callback->Notify(WINHTTP_CALLBACK_STATUS_REQUEST_ERROR,
                         API_READ_DATA, GetLastError());
      break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
      callback->Notify(status, status_info_len, 0);
      break;
    case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
      DCHECK(status_info_len == sizeof(DWORD));
      callback->Notify(status, static_cast<DWORD*>(status_info)[0], 0);
      break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
      callback->Notify(status, TRUE, 0);
      break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      callback->Notify(status, TRUE, 0);
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
      WINHTTP_ASYNC_RESULT* result =
          static_cast<WINHTTP_ASYNC_RESULT*>(status_info);
      callback->Notify(status, result->dwResult, result->dwError);
      if (API_SEND_REQUEST == result->dwResult &&
          ERROR_WINHTTP_NAME_NOT_RESOLVED == result->dwError)
        DidFinishDnsResolutionWithStatus(false,
                                         GURL(),  // null referrer URL.
                                         reinterpret_cast<void*>(context));
      break;
    }
    // This status callback provides the detailed reason for a secure
    // failure.  We map that to an error code and save it for later use.
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE: {
      DCHECK(status_info_len == sizeof(DWORD));
      DWORD* status_ptr = static_cast<DWORD*>(status_info);
      callback->set_secure_failure(*status_ptr);
      break;
    }
    // Looking up the IP address of a server name. The status_info
    // parameter contains a pointer to the server name being resolved.
    case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME: {
      callback->set_load_state(LOAD_STATE_RESOLVING_HOST);
      std::wstring wname(static_cast<wchar_t*>(status_info),
                         status_info_len - 1);
      DidStartDnsResolution(WideToASCII(wname),
                            reinterpret_cast<void*>(context));
      break;
    }
    // Successfully found the IP address of the server.
    case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
      DidFinishDnsResolutionWithStatus(true,
                                       GURL(),  // null referrer URL.
                                       reinterpret_cast<void*>(context));
      break;
  }
}

// Factory --------------------------------------------------------------------

HttpTransactionWinHttp::Factory::~Factory() {
  if (session_)
    session_->Release();
}

HttpTransaction* HttpTransactionWinHttp::Factory::CreateTransaction() {
  if (is_suspended_)
    return NULL;

  if (!session_) {
    session_ = new Session(proxy_service_);
    session_->AddRef();
  }
  return new HttpTransactionWinHttp(session_, proxy_service_->proxy_info());
}

HttpCache* HttpTransactionWinHttp::Factory::GetCache() {
  return NULL;
}

void HttpTransactionWinHttp::Factory::Suspend(bool suspend) {
  is_suspended_ = suspend;

  if (is_suspended_ && session_) {
    session_->Release();
    session_ = NULL;
  }
}

// Transaction ----------------------------------------------------------------

HttpTransactionWinHttp::HttpTransactionWinHttp(Session* session,
                                               const ProxyInfo* info)
    : session_(session),
      request_(NULL),
      load_flags_(0),
      last_error_(ERROR_SUCCESS),
      content_length_remaining_(-1),
      pac_request_(NULL),
      proxy_callback_(this, &HttpTransactionWinHttp::OnProxyInfoAvailable),
      callback_(NULL),
      upload_progress_(0),
      connect_handle_(NULL),
      request_handle_(NULL),
      is_https_(false),
      is_tls_intolerant_(false),
      rev_checking_enabled_(false),
      have_proxy_info_(false),
      need_to_wait_for_handle_closing_(false),
      request_submitted_(false),
      used_embedded_credentials_(false) {
  session->AddRef();
  session_callback_ = new SessionCallback(this, session);
  if (info) {
    proxy_info_.Use(*info);
    have_proxy_info_ = true;
  }
}

HttpTransactionWinHttp::~HttpTransactionWinHttp() {
  if (pac_request_)
    session_->proxy_service()->CancelPacRequest(pac_request_);

  if (request_handle_) {
    if (need_to_wait_for_handle_closing_) {
      session_callback_->set_handle_closing_event(
          session_->handle_closing_event());
    }
    WinHttpCloseHandle(request_handle_);
    if (need_to_wait_for_handle_closing_)
      WaitForSingleObject(session_->handle_closing_event(), INFINITE);
  }
  if (connect_handle_)
    WinHttpCloseHandle(connect_handle_);

  if (request_submitted_) {
    session_->request_throttle()->RemoveRequest(connect_peer_,
                                                request_handle_);
  }

  if (session_callback_) {
    session_callback_->DropTransaction();
    session_callback_ = NULL;  // Release() reference as side effect.
  }
  if (session_)
    session_->Release();
}

int HttpTransactionWinHttp::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback) {
  DCHECK(request_info);
  DCHECK(callback);

  // ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  LOG(INFO) << request_info->method << ": " << request_info->url;

  request_ = request_info;
  load_flags_ = request_info->load_flags;

  int rv = OK;
  if (!have_proxy_info_) {
    // Resolve proxy info.
    rv = session_->proxy_service()->ResolveProxy(request_->url,
                                                 &proxy_info_,
                                                 &proxy_callback_,
                                                 &pac_request_);
    if (rv == ERR_IO_PENDING) {
      session_callback_->set_load_state(
          LOAD_STATE_RESOLVING_PROXY_FOR_URL);
    }
  }

  if (rv == OK)
    rv = DidResolveProxy();  // calls OpenRequest and SendRequest

  if (rv == ERR_IO_PENDING) {
    session_callback_->AddRef();  // balanced when callback runs or from
                                  // OnProxyInfoAvailable.
    callback_ = callback;
  }

  return rv;
}

int HttpTransactionWinHttp::RestartIgnoringLastError(
    CompletionCallback* callback) {
  int flags = load_flags_;

  // Depending on the error, we make different adjustments to our load flags.
  // We DCHECK that we shouldn't already have ignored this error.
  switch (last_error_) {
    case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:
      DCHECK(!(flags & LOAD_IGNORE_CERT_COMMON_NAME_INVALID));
      flags |= LOAD_IGNORE_CERT_COMMON_NAME_INVALID;
      break;
    case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:
      DCHECK(!(flags & LOAD_IGNORE_CERT_DATE_INVALID));
      flags |= LOAD_IGNORE_CERT_DATE_INVALID;
      break;
    case ERROR_WINHTTP_SECURE_INVALID_CA:
      DCHECK(!(flags & LOAD_IGNORE_CERT_AUTHORITY_INVALID));
      flags |= LOAD_IGNORE_CERT_AUTHORITY_INVALID;
      break;
    case ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE:
      DCHECK(!(flags & LOAD_IGNORE_CERT_WRONG_USAGE));
      flags |= LOAD_IGNORE_CERT_WRONG_USAGE;
      break;
    case ERROR_WINHTTP_SECURE_CERT_REV_FAILED: {
      DCHECK(!(flags & LOAD_IGNORE_CERT_REVOCATION));
      flags |= LOAD_IGNORE_CERT_REVOCATION;
      // WinHTTP doesn't have a SECURITY_FLAG_IGNORE_CERT_REV_FAILED flag
      // and doesn't let us undo WINHTTP_ENABLE_SSL_REVOCATION.  The only
      // way to ignore this error is to open a new request without enabling
      // WINHTTP_ENABLE_SSL_REVOCATION.
      if (!ReopenRequest())
        return TranslateLastOSError();
      break;
    }
    // We can't instruct WinHttp to recover from these errors.  No choice
    // but to cancel the request.
    case ERROR_WINHTTP_SECURE_CHANNEL_ERROR:
    case ERROR_WINHTTP_SECURE_INVALID_CERT:
    case ERROR_WINHTTP_SECURE_CERT_REVOKED:
    // We don't knows how to continue from here.
    default:
      LOG(ERROR) << "Unable to restart the HTTP transaction ignoring "
                    "the error " << last_error_;
      return ERR_ABORTED;
  }

  // Update the load flags to ignore the specified error.
  load_flags_ = flags;

  return Restart(callback);
}

int HttpTransactionWinHttp::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  DCHECK(proxy_auth_ && proxy_auth_->state == AUTH_STATE_NEED_AUTH ||
         server_auth_ && server_auth_->state == AUTH_STATE_NEED_AUTH);

  // Proxy gets set first, then WWW.
  AuthData* auth =
      proxy_auth_ && proxy_auth_->state == AUTH_STATE_NEED_AUTH ?
      proxy_auth_ : server_auth_;

  if (auth) {
    auth->state = AUTH_STATE_HAVE_AUTH;
    auth->username = username;
    auth->password = password;
  }

  return Restart(callback);
}

// The code common to RestartIgnoringLastError and RestartWithAuth.
int HttpTransactionWinHttp::Restart(CompletionCallback* callback) {
  DCHECK(callback);

  // ensure that we only have one asynchronous call at a time.
  DCHECK(!callback_);

  int rv = RestartInternal();
  if (rv != ERR_IO_PENDING)
    return rv;

  session_callback_->AddRef();  // balanced when callback runs.

  callback_ = callback;
  return ERR_IO_PENDING;
}

// If HttpTransactionWinHttp needs to restart itself after handling an error,
// it calls this method.  This method leaves callback_ unchanged.  The caller
// is responsible for calling session_callback_->AddRef() if this method
// returns ERR_IO_PENDING.
int HttpTransactionWinHttp::RestartInternal() {
  content_length_remaining_ = -1;
  upload_progress_ = 0;

  return SendRequest();
}

// We use WinHttpQueryDataAvailable rather than pure async read to trade
// a better latency for a decreased throughput.  We'll make more IO calls,
// and thus use more CPU for a given transaction by using
// WinHttpQueryDataAvailable, but it allows us to get a faster response
// time to the app for data, which is more important.
int HttpTransactionWinHttp::Read(char* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(buf_len > 0);
  DCHECK(callback);

  DCHECK(!callback_);
  DCHECK(request_handle_);

  // If we have already received the full response, then we know we are done.
  if (content_length_remaining_ == 0) {
    LogTransactionMetrics();
    return 0;
  }

  session_callback_->set_read_buf(buf);
  session_callback_->set_read_buf_len(buf_len);

  // We must consume all the available data reported by the previous
  // WinHttpQueryDataAvailable call before we can call
  // WinHttpQueryDataAvailable again.
  BOOL ok;
  if (session_callback_->bytes_available()) {
    ok = session_callback_->ReadData(request_handle_);
  } else {
    ok = WinHttpQueryDataAvailable(request_handle_, NULL);
  }
  if (!ok)
    return TranslateLastOSError();

  session_callback_->set_load_state(LOAD_STATE_READING_RESPONSE);
  session_callback_->AddRef();  // balanced when callback runs.
  need_to_wait_for_handle_closing_ = true;

  callback_ = callback;
  return ERR_IO_PENDING;
}

const HttpResponseInfo* HttpTransactionWinHttp::GetResponseInfo() const {
  return (response_.headers || response_.ssl_info.cert) ? &response_ : NULL;
}

LoadState HttpTransactionWinHttp::GetLoadState() const {
  return session_callback_->load_state();
}

uint64 HttpTransactionWinHttp::GetUploadProgress() const {
  return upload_progress_;
}

void HttpTransactionWinHttp::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(callback_);

  // since Run may result in Read being called, clear callback_ up front.
  CompletionCallback* c = callback_;
  callback_ = NULL;
  c->Run(rv);
}

bool HttpTransactionWinHttp::OpenRequest() {
  DCHECK(!connect_handle_);
  DCHECK(!request_handle_);

  const GURL& url = request_->url;
  const std::string& scheme = url.scheme();

  // Flags passed to WinHttpOpenRequest.  Disable any conversion WinHttp
  // might perform on our URL string.  We handle the escaping ourselves.
  DWORD open_flags = WINHTTP_FLAG_ESCAPE_DISABLE |
                     WINHTTP_FLAG_ESCAPE_DISABLE_QUERY |
                     WINHTTP_FLAG_NULL_CODEPAGE;

  // We should only be dealing with HTTP at this point:
  DCHECK(LowerCaseEqualsASCII(scheme, "http") ||
         LowerCaseEqualsASCII(scheme, "https"));

  int in_port = url.IntPort();
  DCHECK(in_port != url_parse::PORT_INVALID) <<
      "Valid URLs should have valid ports";

  // Map to port numbers that Windows expects.
  INTERNET_PORT port = in_port;
  if (LowerCaseEqualsASCII(scheme, "https")) {
    is_https_ = true;
    open_flags |= WINHTTP_FLAG_SECURE;
    if (in_port == url_parse::PORT_UNSPECIFIED)
      port = INTERNET_DEFAULT_HTTPS_PORT;
  } else {
    if (in_port == url_parse::PORT_UNSPECIFIED)
      port = INTERNET_DEFAULT_HTTP_PORT;
  }

  const std::string& host = url.host();

  // Use the primary session handle unless we are talking to a TLS-intolerant
  // server.
  //
  // Since the SSL protocol versions enabled are an option of a session
  // handle, supporting TLS-intolerant servers unfortunately requires opening
  // an alternative session in which TLS 1.0 is disabled.
  if (!session_->internet() && !session_->Init(request_->user_agent)) {
    DLOG(ERROR) << "unable to create the internet";
    return false;
  }
  HINTERNET internet = session_->internet();
  if (is_tls_intolerant_) {
    if (!session_->internet_no_tls() &&
        !session_->InitNoTLS(request_->user_agent)) {
      DLOG(ERROR) << "unable to create the no-TLS alternative internet";
      return false;
    }
    internet = session_->internet_no_tls();
  }

  // This function operates synchronously.
  connect_handle_ =
      WinHttpConnect(internet, ASCIIToWide(host).c_str(), port, 0);
  if (!connect_handle_) {
    DLOG(ERROR) << "WinHttpConnect failed: " << GetLastError();
    return false;
  }

  std::string request_path = url.PathForRequest();

  // This function operates synchronously.
  request_handle_ =
      WinHttpOpenRequest(connect_handle_,
                         ASCIIToWide(request_->method).c_str(),
                         ASCIIToWide(request_path).c_str(),
                         NULL,  // use HTTP/1.1
                         WINHTTP_NO_REFERER,  // none
                         WINHTTP_DEFAULT_ACCEPT_TYPES,  // none
                         open_flags);
  if (!request_handle_) {
    DLOG(ERROR) << "WinHttpOpenRequest failed: " << GetLastError();
    return false;
  }

  // TODO(darin): we may wish to prune-back the set of notifications we receive
  WINHTTP_STATUS_CALLBACK old_callback = WinHttpSetStatusCallback(
      request_handle_, StatusCallback,
      WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, NULL);
  DCHECK(old_callback == NULL);
  if (old_callback == WINHTTP_INVALID_STATUS_CALLBACK) {
    DLOG(ERROR) << "WinHttpSetStatusCallback failed:" << GetLastError();
    return false;
  }

  DWORD_PTR ctx = reinterpret_cast<DWORD_PTR>(session_callback_.get());
  if (!WinHttpSetOption(request_handle_, WINHTTP_OPTION_CONTEXT_VALUE,
                        &ctx, sizeof(ctx))) {
    DLOG(ERROR) << "WinHttpSetOption context value failed:" << GetLastError();
    return false;
  }

  // We just associated a status callback context value with the request
  // handle.
  session_callback_->AddRef();  // balanced in OnHandleClosing
  session_->AddRefBySessionCallback();

  // We have our own cookie and redirect management.
  DWORD options = WINHTTP_DISABLE_COOKIES |
                  WINHTTP_DISABLE_REDIRECTS;

  if (!WinHttpSetOption(request_handle_, WINHTTP_OPTION_DISABLE_FEATURE,
                        &options, sizeof(options))) {
    DLOG(ERROR) << "WinHttpSetOption disable feature failed:" << GetLastError();
    return false;
  }

  // Disable auto-login for Negotiate and NTLM auth methods.
  DWORD security_level = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
  if (!WinHttpSetOption(request_handle_, WINHTTP_OPTION_AUTOLOGON_POLICY,
                        &security_level, sizeof(security_level))) {
    DLOG(ERROR) << "WinHttpSetOption autologon failed: " << GetLastError();
    return false;
  }

  // Add request headers.  WinHttp is known to convert the headers to bytes
  // using the system charset converter, so we use the same converter to map
  // our request headers to UTF-16 before handing the data to WinHttp.
  std::wstring request_headers = base::SysNativeMBToWide(GetRequestHeaders());

  DWORD len = static_cast<DWORD>(request_headers.size());
  if (!WinHttpAddRequestHeaders(request_handle_,
                                request_headers.c_str(),
                                len,
                                WINHTTP_ADDREQ_FLAG_ADD |
                                WINHTTP_ADDREQ_FLAG_REPLACE)) {
    DLOG(ERROR) << "WinHttpAddRequestHeaders failed: " << GetLastError();
    return false;
  }

  return true;
}

int HttpTransactionWinHttp::SendRequest() {
  DCHECK(request_handle_);

  // Apply any authentication (username/password) we might have.
  ApplyAuth();

  // Apply any proxy info.
  proxy_info_.Apply(request_handle_);

  // Check SSL server certificate revocation.
  if (is_https_) {
    bool ignore_cert_rev = (load_flags_ & LOAD_IGNORE_CERT_REVOCATION) != 0;
    GURL origin = request_->url.GetOrigin();
    const std::string& origin_spec = origin.spec();
    if (ignore_cert_rev)
      session_->IgnoreCertRev(origin_spec);
    else if (session_->ShouldIgnoreCertRev(origin_spec))
      ignore_cert_rev = true;

    if (session_->rev_checking_enabled() && !ignore_cert_rev) {
      DWORD options = WINHTTP_ENABLE_SSL_REVOCATION;
      if (!WinHttpSetOption(request_handle_, WINHTTP_OPTION_ENABLE_FEATURE,
                            &options, sizeof(options))) {
        DLOG(ERROR) << "WinHttpSetOption failed: " << GetLastError();
        return TranslateLastOSError();
      }
      rev_checking_enabled_ = true;
    }
  }

  const int kCertFlags = LOAD_IGNORE_CERT_COMMON_NAME_INVALID |
                         LOAD_IGNORE_CERT_DATE_INVALID |
                         LOAD_IGNORE_CERT_AUTHORITY_INVALID |
                         LOAD_IGNORE_CERT_WRONG_USAGE;

  if (load_flags_ & kCertFlags) {
    DWORD security_flags;
    DWORD length = sizeof(security_flags);

    if (!WinHttpQueryOption(request_handle_,
                            WINHTTP_OPTION_SECURITY_FLAGS,
                            &security_flags,
                            &length)) {
      NOTREACHED() << "WinHttpQueryOption failed.";
      return TranslateLastOSError();
    }

    // On Vista, WinHttpSetOption() fails with an incorrect parameter error.
    // WinHttpQueryOption() sets an undocumented flag (0x01000000, which seems
    // to be a query-only flag) in security_flags that causes this error.  To
    // work-around it, we only keep the documented error flags.
    security_flags &= (SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                       SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                       SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                       SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE);

    if (load_flags_ & LOAD_IGNORE_CERT_COMMON_NAME_INVALID)
      security_flags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;

    if (load_flags_ & LOAD_IGNORE_CERT_DATE_INVALID)
      security_flags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

    if (load_flags_ & LOAD_IGNORE_CERT_AUTHORITY_INVALID)
      security_flags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;

    if (load_flags_ & LOAD_IGNORE_CERT_WRONG_USAGE)
      security_flags |= SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

    if (!WinHttpSetOption(request_handle_,
                          WINHTTP_OPTION_SECURITY_FLAGS,
                          &security_flags,
                          sizeof(security_flags))) {
      NOTREACHED() << "WinHttpSetOption failed.";
      return TranslateLastOSError();
    }
  }

  response_.request_time = Time::Now();
  response_.was_cached = false;

  DWORD total_size = 0;
  if (request_->upload_data) {
    upload_stream_.reset(new UploadDataStream(request_->upload_data));
    uint64 upload_len = upload_stream_->size();
    if (upload_len == 0) {
      upload_stream_.reset();
    } else {
      // TODO(darin): no way to support >4GB uploads w/ WinHttp?
      if (upload_len > static_cast<uint64>(DWORD(-1))) {
        NOTREACHED() << "upload length is too large";
        return ERR_FILE_TOO_BIG;
      }

      total_size = static_cast<DWORD>(upload_len);
    }
  }

  if (request_submitted_) {
    request_submitted_ = false;
    session_->request_throttle()->NotifyRequestDone(connect_peer_);
  }
  if (proxy_info_.is_direct())
    connect_peer_ = request_->url.GetOrigin().spec();
  else
    connect_peer_ = proxy_info_.proxy_server();
  DWORD_PTR ctx = reinterpret_cast<DWORD_PTR>(session_callback_.get());
  if (!session_->request_throttle()->SubmitRequest(connect_peer_,
                                                   request_handle_,
                                                   total_size, ctx)) {
    last_error_ = GetLastError();
    DLOG(ERROR) << "WinHttpSendRequest failed: " << last_error_;
    return TranslateOSError(last_error_);
  }

  request_submitted_ = true;
  return ERR_IO_PENDING;
}

// Called after certain failures of SendRequest to reset the members opened
// or modified in OpenRequest and SendRequest and call OpenRequest again.
bool HttpTransactionWinHttp::ReopenRequest() {
  DCHECK(connect_handle_);
  DCHECK(request_handle_);

  session_callback_->set_handle_closing_event(
      session_->handle_closing_event());
  WinHttpCloseHandle(request_handle_);
  WaitForSingleObject(session_->handle_closing_event(), INFINITE);
  request_handle_ = NULL;
  WinHttpCloseHandle(connect_handle_);
  connect_handle_ = NULL;
  session_callback_->ResetForNewRequest();

  // Don't need to reset is_https_, rev_checking_enabled_, and
  // response_.request_time.

  return OpenRequest();
}

int HttpTransactionWinHttp::DidResolveProxy() {
  // We may already have a request handle if we are changing proxy config.
  if (!(request_handle_ ? ReopenRequest() : OpenRequest()))
    return TranslateLastOSError();

  return SendRequest();
}

int HttpTransactionWinHttp::DidReceiveError(DWORD error,
                                            DWORD secure_failure) {
  DCHECK(error != ERROR_SUCCESS);

  session_callback_->set_load_state(LOAD_STATE_IDLE);
  need_to_wait_for_handle_closing_ = false;

  int rv;

  if (error == ERROR_WINHTTP_RESEND_REQUEST)
    return RestartInternal();

  if (error == ERROR_WINHTTP_NAME_NOT_RESOLVED ||
      error == ERROR_WINHTTP_CANNOT_CONNECT ||
      error == ERROR_WINHTTP_TIMEOUT) {
    // These errors may have been caused by a proxy configuration error, or
    // rather they may go away by trying a different proxy config!  If we have
    // an explicit proxy config, then we just have to report an error.
    if (!have_proxy_info_) {
      rv = session_->proxy_service()->ReconsiderProxyAfterError(
          request_->url, &proxy_info_, &proxy_callback_, &pac_request_);
      if (rv == OK)  // got new proxy info to try
        return DidResolveProxy();
      if (rv == ERR_IO_PENDING)  // waiting to resolve proxy info
        return rv;
      // else, fall through and just report an error.
    }
  }

  if (error == ERROR_WINHTTP_SECURE_FAILURE) {
    DWORD filtered_secure_failure = FilterSecureFailure(secure_failure,
                                                        load_flags_);
    // If load_flags_ ignores all the errors in secure_failure, we shouldn't
    // get the ERROR_WINHTTP_SECURE_FAILURE error.
    DCHECK(filtered_secure_failure || !secure_failure);
    error = MapSecureFailureToError(filtered_secure_failure);
  }

  last_error_ = error;
  rv = TranslateOSError(error);

  if ((rv == ERR_SSL_PROTOCOL_ERROR ||
       rv == ERR_SSL_VERSION_OR_CIPHER_MISMATCH) &&
      !session_callback_->request_was_probably_sent() &&
      session_->tls_enabled() && !is_tls_intolerant_) {
    // The server might be TLS intolerant.  Or it might be an SSL 3.0 server
    // that chose a TLS-only cipher suite, which we handle in the same way.
    // Downgrade to SSL 3.0 and retry.
    is_tls_intolerant_ = true;
    if (!ReopenRequest())
      return TranslateLastOSError();
    return RestartInternal();
  }
  if (rv == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    // TODO(wtc): Bug 1230409: We don't support SSL client authentication yet.
    // For now we set a null client certificate, which works on XP SP3, Vista
    // and later.  On XP SP2 and below, this fails with ERROR_INVALID_PARAMETER
    // (87).  This allows us to access servers that request but do not require
    // client certificates.
    if (WinHttpSetOption(request_handle_,
                         WINHTTP_OPTION_CLIENT_CERT_CONTEXT,
                         WINHTTP_NO_CLIENT_CERT_CONTEXT, 0))
      return RestartInternal();
  }
  if (IsCertificateError(rv)) {
    response_.ssl_info.cert = GetServerCertificate();
    response_.ssl_info.cert_status =
        MapSecureFailureToCertStatus(secure_failure);
    CertStatusCache* cert_status_cache = session_->cert_status_cache();
    cert_status_cache->SetCertStatus(*response_.ssl_info.cert,
                                     request_->url.host(),
                                     response_.ssl_info.cert_status);
  }

  return rv;
}

int HttpTransactionWinHttp::DidSendRequest() {
  BOOL ok;
  if (upload_stream_.get() && upload_stream_->buf_len() > 0) {
    // write upload data
    DWORD buf_len = static_cast<DWORD>(upload_stream_->buf_len());
    ok = WinHttpWriteData(request_handle_,
                          upload_stream_->buf(),
                          buf_len,
                          NULL);
    if (ok)
      need_to_wait_for_handle_closing_ = true;
  } else {
    upload_stream_.reset();
    need_to_wait_for_handle_closing_ = false;

    // begin receiving the response
    ok = WinHttpReceiveResponse(request_handle_, NULL);
  }
  return ok ? ERR_IO_PENDING : TranslateLastOSError();
}

int HttpTransactionWinHttp::DidWriteData(DWORD num_bytes) {
  DCHECK(upload_stream_.get());
  DCHECK(num_bytes > 0);

  upload_stream_->DidConsume(num_bytes);
  upload_progress_ = upload_stream_->position();

  // OK, we are ready to start receiving the response.  The code in
  // DidSendRequest does exactly what we want!
  return DidSendRequest();
}

int HttpTransactionWinHttp::DidReadData(DWORD num_bytes) {
  int rv = static_cast<int>(num_bytes);
  DCHECK(rv >= 0);

  session_callback_->set_load_state(LOAD_STATE_IDLE);
  session_callback_->reduce_bytes_available(rv);
  need_to_wait_for_handle_closing_ = false;

  if (content_length_remaining_ > 0) {
    content_length_remaining_ -= rv;

    // HTTP/1.0 servers are known to send more data than they report in their
    // Content-Length header (in the non-keepalive case).  IE and Moz both
    // tolerate this situation, and therefore so must we.
    if (content_length_remaining_ < 0)
      content_length_remaining_ = 0;
  }

  // We have read the entire response.  Mark the request done to unblock a
  // queued request.
  if (rv == 0) {
    LogTransactionMetrics();
    DCHECK(request_submitted_);
    request_submitted_ = false;
    session_->request_throttle()->NotifyRequestDone(connect_peer_);
  }

  return rv;
}

void HttpTransactionWinHttp::LogTransactionMetrics() const {
  base::TimeDelta duration = base::Time::Now() - response_.request_time;
  if (60 < duration.InMinutes())
    return;
  UMA_HISTOGRAM_LONG_TIMES(L"Net.Transaction_Latency_WinHTTP", duration);
}

int HttpTransactionWinHttp::DidReceiveHeaders() {
  session_callback_->set_load_state(LOAD_STATE_IDLE);

  DWORD size = 0;
  if (!WinHttpQueryHeaders(request_handle_,
                           WINHTTP_QUERY_RAW_HEADERS,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           NULL,
                           &size,
                           WINHTTP_NO_HEADER_INDEX)) {
    DWORD error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER) {
      DLOG(ERROR) << "WinHttpQueryHeaders failed: " << GetLastError();
      return TranslateLastOSError();
    }
    // OK, size should tell us how much to allocate...
    DCHECK(size > 0);
  }

  std::wstring raw_headers;

  // 'size' is the number of bytes rather than the number of characters.
  DCHECK(size % 2 == 0);
  if (!WinHttpQueryHeaders(request_handle_,
                           WINHTTP_QUERY_RAW_HEADERS,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           WriteInto(&raw_headers, size/2 + 1),
                           &size,
                           WINHTTP_NO_HEADER_INDEX)) {
    DLOG(ERROR) << "WinHttpQueryHeaders failed: " << GetLastError();
    return TranslateLastOSError();
  }

  response_.response_time = Time::Now();

  // From experimentation, it appears that WinHttp translates non-ASCII bytes
  // found in the response headers to UTF-16 assuming that they are encoded
  // using the default system charset.  We attempt to undo that here.
  response_.headers =
      new HttpResponseHeaders(base::SysWideToNativeMB(raw_headers));

  // WinHTTP truncates a response longer than 2GB.  Perhaps it stores the
  // response's content length in a signed 32-bit integer.  We fail rather
  // than reading a truncated response.
  if (response_.headers->GetContentLength() > 0x80000000)
    return ERR_FILE_TOO_BIG;

  response_.vary_data.Init(*request_, *response_.headers);
  int rv = PopulateAuthChallenge();
  if (rv != OK)
    return rv;

  // Unfortunately, WinHttp does not close the connection when a non-keepalive
  // response is _not_ followed by the server closing the connection.  So, we
  // attempt to hack around this bug.
  if (!response_.headers->IsKeepAlive())
    content_length_remaining_ = response_.headers->GetContentLength();

  return OK;
}

// Populates response_.auth_challenge with the authentication challenge info.
int HttpTransactionWinHttp::PopulateAuthChallenge() {
  DCHECK(response_.headers);

  int status = response_.headers->response_code();
  if (status != 401 && status != 407)
    return OK;

  scoped_refptr<AuthChallengeInfo> auth_info = new AuthChallengeInfo;

  auth_info->is_proxy = (status == 407);

  if (auth_info->is_proxy) {
    // TODO(wtc): get the proxy server host from proxy_info_.
    // TODO(wtc): internationalize?
    auth_info->host = L"proxy";
  } else {
    auth_info->host = ASCIIToWide(request_->url.host());
  }

  // Here we're checking only the first *-Authenticate header.  When a server
  // responds with multiple methods, we use the first.
  // TODO(wtc): Bug 1124614: look at all the authentication methods and pick
  // the best one that we support.  failover to other authentication methods.
  std::string header_value;
  std::string header_name = auth_info->is_proxy ?
      "Proxy-Authenticate" : "WWW-Authenticate";
  if (!response_.headers->EnumerateHeader(NULL, header_name, &header_value))
    return OK;

  // TODO(darin): Need to support RFC 2047 encoded realm strings.  For now, we
  // limit our support to ASCII and "native code page" realm strings.
  std::wstring auth_header = base::SysNativeMBToWide(header_value);

  // auth_header is a string which looks like:
  // Digest realm="The Awesome Site", domain="/page.html", ...
  std::wstring::const_iterator space = find(auth_header.begin(),
                                            auth_header.end(), L' ');
  auth_info->scheme.assign(auth_header.begin(), space);
  auth_info->realm = GetHeaderParamValue(auth_header, L"realm");

  // Now auth_info has been fully populated.  Before we swap it with
  // response_.auth_challenge, update the auth cache key and remove any
  // presumably incorrect auth data in the auth cache.
  std::string* auth_cache_key;
  AuthData* auth;
  if (auth_info->is_proxy) {
    if (!proxy_auth_)
      proxy_auth_ = new AuthData;
    auth = proxy_auth_;
    auth_cache_key = &proxy_auth_cache_key_;
  } else {
    if (!server_auth_)
      server_auth_ = new AuthData;
    auth = server_auth_;
    auth_cache_key = &server_auth_cache_key_;
  }
  *auth_cache_key = AuthCache::HttpKey(request_->url, *auth_info);
  DCHECK(!auth_cache_key->empty());
  auth->scheme = auth_info->scheme;
  if (auth->state == AUTH_STATE_HAVE_AUTH) {
    // Remove the cache entry for the credentials we just failed on.
    // Note: we require the username/password to match before removing
    // since the entry in the cache may be newer than what we used last time.
    AuthData* cached_auth = session_->auth_cache()->Lookup(*auth_cache_key);
    if (cached_auth && cached_auth->username == auth->username &&
        cached_auth->password == auth->password)
      session_->auth_cache()->Remove(*auth_cache_key);
    auth->state = AUTH_STATE_NEED_AUTH;
  }
  DCHECK(auth->state == AUTH_STATE_NEED_AUTH);

  // Try to use the username/password embedded in the URL first.
  // (By checking !used_embedded_credentials_, we make sure that this
  // is only done once for the transaction.)
  if (!auth_info->is_proxy && request_->url.has_username() &&
      !used_embedded_credentials_) {
    // TODO(wtc) It may be necessary to unescape the username and password
    // after extracting them from the URL.  We should be careful about
    // embedded nulls in that case.
    used_embedded_credentials_ = true;
    auth->state = AUTH_STATE_HAVE_AUTH;
    auth->username = ASCIIToWide(request_->url.username());
    auth->password = ASCIIToWide(request_->url.password());
    return RestartInternal();
  }

  // Check the auth cache for an entry.
  AuthData* cached_auth = session_->auth_cache()->Lookup(*auth_cache_key);
  if (cached_auth) {
    auth->state = AUTH_STATE_HAVE_AUTH;
    auth->username = cached_auth->username;
    auth->password = cached_auth->password;
    return RestartInternal();
  }

  response_.auth_challenge.swap(auth_info);
  return OK;
}

static DWORD StringToAuthScheme(const std::wstring& scheme) {
  if (LowerCaseEqualsASCII(scheme, "basic"))
    return WINHTTP_AUTH_SCHEME_BASIC;
  if (LowerCaseEqualsASCII(scheme, "digest"))
    return WINHTTP_AUTH_SCHEME_DIGEST;
  if (LowerCaseEqualsASCII(scheme, "ntlm"))
    return WINHTTP_AUTH_SCHEME_NTLM;
  if (LowerCaseEqualsASCII(scheme, "negotiate"))
    return WINHTTP_AUTH_SCHEME_NEGOTIATE;
  if (LowerCaseEqualsASCII(scheme, "passport1.4"))
    return WINHTTP_AUTH_SCHEME_PASSPORT;
  return 0;
}

// Applies authentication credentials to request_handle_.
void HttpTransactionWinHttp::ApplyAuth() {
  DWORD auth_scheme;
  BOOL rv;
  if (proxy_auth_ && proxy_auth_->state == AUTH_STATE_HAVE_AUTH) {
    // Add auth data to cache.
    DCHECK(!proxy_auth_cache_key_.empty());
    session_->auth_cache()->Add(proxy_auth_cache_key_, proxy_auth_);
    auth_scheme = StringToAuthScheme(proxy_auth_->scheme);
    if (auth_scheme == 0)
      return;

    rv = WinHttpSetCredentials(request_handle_,
                               WINHTTP_AUTH_TARGET_PROXY,
                               auth_scheme,
                               proxy_auth_->username.c_str(),
                               proxy_auth_->password.c_str(),
                               NULL);
  }

  if (server_auth_ && server_auth_->state == AUTH_STATE_HAVE_AUTH) {
    // Add auth data to cache.
    DCHECK(!server_auth_cache_key_.empty());
    session_->auth_cache()->Add(server_auth_cache_key_, server_auth_);
    auth_scheme = StringToAuthScheme(server_auth_->scheme);
    if (auth_scheme == 0)
      return;

    rv = WinHttpSetCredentials(request_handle_,
                               WINHTTP_AUTH_TARGET_SERVER,
                               auth_scheme,
                               server_auth_->username.c_str(),
                               server_auth_->password.c_str(),
                               NULL);
  }
}

void HttpTransactionWinHttp::OnProxyInfoAvailable(int result) {
  if (result != OK) {
    DLOG(WARNING) << "failed to get proxy info: " << result;
    proxy_info_.UseDirect();
  }

  // Balances extra reference taken when proxy resolution was initiated.
  session_callback_->Release();

  pac_request_ = NULL;

  // Since OnProxyInfoAvailable is always called asynchronously (via the
  // message loop), we need to trap any errors and pass them to the consumer
  // via their completion callback.

  int rv = DidResolveProxy();
  if (rv == ERR_IO_PENDING) {
    session_callback_->AddRef();  // balanced when callback runs.
  } else {
    DoCallback(rv);
  }
}

std::string HttpTransactionWinHttp::GetRequestHeaders() const {
  std::string headers;

  if (!request_->user_agent.empty())
    headers += "User-Agent: " + request_->user_agent + "\r\n";

  // Our consumer should have made sure that this is a safe referrer.  See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (request_->referrer.is_valid())
    headers += "Referer: " + request_->referrer.spec() + "\r\n";

  // IE and Safari do this.  Presumably it is to support sending a HEAD request
  // to an URL that only expects to be sent a POST or some other method that
  // normally would have a message body.
  if (request_->method == "HEAD")
    headers += "Content-Length: 0\r\n";

  // Honor load flags that impact proxy caches.
  if (request_->load_flags & LOAD_BYPASS_CACHE) {
    headers += "Pragma: no-cache\r\nCache-Control: no-cache\r\n";
  } else if (request_->load_flags & LOAD_VALIDATE_CACHE) {
    headers += "Cache-Control: max-age=0\r\n";
  }

  // TODO(darin): Prune out duplicate headers?
  headers += request_->extra_headers;

  return headers;
}

// Retrieves the SSL server certificate associated with the transaction.
// The caller is responsible for freeing the certificate.
X509Certificate* HttpTransactionWinHttp::GetServerCertificate() const {
  DCHECK(is_https_);
  PCCERT_CONTEXT cert_context = NULL;
  DWORD length = sizeof(cert_context);
  if (!WinHttpQueryOption(request_handle_,
                          WINHTTP_OPTION_SERVER_CERT_CONTEXT,
                          &cert_context,
                          &length)) {
    return NULL;
  }
  // cert_context may be NULL here even though WinHttpQueryOption succeeded.
  // For example, a proxy server may return a 404 error page to report the
  // DNS resolution failure of the server's hostname.
  if (!cert_context)
    return NULL;
  return X509Certificate::CreateFromHandle(cert_context);
}

// Retrieves the security strength, in bits, of the SSL cipher suite
// associated with the transaction.
int HttpTransactionWinHttp::GetSecurityBits() const {
  DCHECK(is_https_);
  DWORD key_bits = 0;
  DWORD length = sizeof(key_bits);
  if (!WinHttpQueryOption(request_handle_,
                          WINHTTP_OPTION_SECURITY_KEY_BITNESS,
                          &key_bits,
                          &length)) {
    return -1;
  }
  return key_bits;
}

void HttpTransactionWinHttp::PopulateSSLInfo(DWORD secure_failure) {
  if (is_https_) {
    response_.ssl_info.cert = GetServerCertificate();
    response_.ssl_info.security_bits = GetSecurityBits();
    // If there is no cert (such as when the proxy server makes up a
    // 404 response to report a server name resolution error), don't set
    // the cert status.
    if (!response_.ssl_info.cert)
      return;
    response_.ssl_info.cert_status =
        MapSecureFailureToCertStatus(secure_failure);
    // WinHTTP does not always return a cert status once we ignored errors
    // for a cert.  (Our experiments showed that WinHTTP reliably returns a
    // cert status only when there are unignored errors or when we resend a
    // request with the errors ignored.)  So we have to remember what the
    // last status was for a cert.  Note that if the cert status changes
    // from error to OK, we won't know that.  If we have never stored our
    // status in the CertStatusCache (meaning no errors so far), then it is
    // OK (0).
    CertStatusCache* cert_status_cache = session_->cert_status_cache();
    if (net::IsCertStatusError(response_.ssl_info.cert_status)) {
      cert_status_cache->SetCertStatus(*response_.ssl_info.cert,
                                       request_->url.host(),
                                       response_.ssl_info.cert_status);
    } else {
      response_.ssl_info.cert_status |=
          cert_status_cache->GetCertStatus(*response_.ssl_info.cert,
                                           request_->url.host()) &
          net::CERT_STATUS_ALL_ERRORS;
    }

    if (rev_checking_enabled_)
      response_.ssl_info.cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
  } else {
    // If this is not https, we should not get a cert status.
    DCHECK(!secure_failure);
  }
}

void HttpTransactionWinHttp::HandleStatusCallback(DWORD status,
                                                  DWORD_PTR result,
                                                  DWORD error,
                                                  DWORD secure_failure) {
  int rv;

  switch (status) {
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
      rv = DidReceiveError(error, secure_failure);
      break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
      PopulateSSLInfo(secure_failure);
      rv = DidSendRequest();
      break;
    case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
      rv = DidWriteData(static_cast<DWORD>(result));
      break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      rv = DidReceiveHeaders();
      break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
      rv = DidReadData(static_cast<DWORD>(result));
      break;
    default:
      NOTREACHED() << "unexpected status code";
      rv = ERR_UNEXPECTED;
      break;
  }

  if (rv == ERR_IO_PENDING) {
    session_callback_->AddRef();  // balanced when callback runs.
  } else if (callback_) {
    DoCallback(rv);
  }
}

}  // namespace net

