// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
#define NET_HTTP_HTTP_NETWORK_TRANSACTION_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

class ClientSocketFactory;
class HttpChunkedDecoder;
class HttpNetworkSession;
class HttpStream;
class UploadDataStream;

class HttpNetworkTransaction : public HttpTransaction {
 public:
  HttpNetworkTransaction(HttpNetworkSession* session,
                         ClientSocketFactory* socket_factory);

  virtual ~HttpNetworkTransaction();

  // HttpTransaction methods:
  virtual int Start(const HttpRequestInfo* request_info,
                    CompletionCallback* callback);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int RestartWithCertificate(X509Certificate* client_cert,
                                     CompletionCallback* callback);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual bool IsReadyToRestartForAuth() {
    return pending_auth_target_ != HttpAuth::AUTH_NONE &&
        HaveAuth(pending_auth_target_);
  }

  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  FRIEND_TEST(HttpNetworkTransactionTest, ResetStateForRestart);

  // This version of IOBuffer lets us use a string as the real storage and
  // "move" the data pointer inside the string before using it to do actual IO.
  class RequestHeaders : public net::IOBuffer {
   public:
    RequestHeaders() : net::IOBuffer() {}
    ~RequestHeaders() { data_ = NULL; }

    void SetDataOffset(size_t offset) {
      data_ = const_cast<char*>(headers_.data()) + offset;
    }

    // This is intentionally a public member.
    std::string headers_;
  };

  // This version of IOBuffer lets us use a malloc'ed buffer as the real storage
  // and "move" the data pointer inside the buffer before using it to do actual
  // IO.
  class ResponseHeaders : public net::IOBuffer {
   public:
    ResponseHeaders() : net::IOBuffer() {}
    ~ResponseHeaders() { data_ = NULL; }

    void set_data(size_t offset) { data_ = headers_.get() + offset; }
    char* headers() { return headers_.get(); }
    void Reset() { headers_.reset(); }
    void Realloc(size_t new_size);

   private:
    scoped_ptr_malloc<char> headers_;
  };

  enum State {
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,
    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_SOCKS_CONNECT,
    STATE_SOCKS_CONNECT_COMPLETE,
    STATE_SSL_CONNECT,
    STATE_SSL_CONNECT_COMPLETE,
    STATE_WRITE_HEADERS,
    STATE_WRITE_HEADERS_COMPLETE,
    STATE_WRITE_BODY,
    STATE_WRITE_BODY_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE,
    STATE_NONE
  };

  enum ProxyMode {
    kDirectConnection,  // If using a direct connection
    kHTTPProxy,  // If using a proxy for HTTP (not HTTPS)
    kHTTPProxyUsingTunnel,  // If using a tunnel for HTTPS
    kSOCKSProxy,  // If using a SOCKS proxy
  };

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoResolveProxy();
  int DoResolveProxyComplete(int result);
  int DoInitConnection();
  int DoInitConnectionComplete(int result);
  int DoSOCKSConnect();
  int DoSOCKSConnectComplete(int result);
  int DoSSLConnect();
  int DoSSLConnectComplete(int result);
  int DoWriteHeaders();
  int DoWriteHeadersComplete(int result);
  int DoWriteBody();
  int DoWriteBodyComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);
  int DoDrainBodyForAuthRestart();
  int DoDrainBodyForAuthRestartComplete(int result);

  // Record histograms of latency until Connect() completes.
  void LogTCPConnectedMetrics() const;

  // Record histogram of time until first byte of header is received.
  void LogTransactionConnectedMetrics() const;

  // Record histogram of latency (durations until last byte received).
  void LogTransactionMetrics() const;

  // Writes a log message to help debugging in the field when we block a proxy
  // response to a CONNECT request.
  void LogBlockedTunnelResponse(int response_code) const;

  // Called when header_buf_ contains the complete response headers.
  int DidReadResponseHeaders();

  // Called to handle a certificate error.  Returns OK if the error should be
  // ignored.  Otherwise, stores the certificate in response_.ssl_info and
  // returns the same error code.
  int HandleCertificateError(int error);

  // Called to handle a client certificate request.
  int HandleCertificateRequest(int error);

  // Called to possibly recover from an SSL handshake error.  Sets next_state_
  // and returns OK if recovering from the error.  Otherwise, the same error
  // code is returned.
  int HandleSSLHandshakeError(int error);

  // Called to possibly recover from the given error.  Sets next_state_ and
  // returns OK if recovering from the error.  Otherwise, the same error code
  // is returned.
  int HandleIOError(int error);

  // Called when we reached EOF or got an error.  Returns true if we should
  // resend the request.
  bool ShouldResendRequest() const;

  // Resets the connection and the request headers for resend.  Called when
  // ShouldResendRequest() is true.
  void ResetConnectionAndRequestForResend();

  // Called when we encounter a network error that could be resolved by trying
  // a new proxy configuration.  If there is another proxy configuration to try
  // then this method sets next_state_ appropriately and returns either OK or
  // ERR_IO_PENDING depending on whether or not the new proxy configuration is
  // available synchronously or asynchronously.  Otherwise, the given error
  // code is simply returned.
  int ReconsiderProxyAfterError(int error);

  // Decides the policy when the connection is closed before the end of headers
  // has been read. This only applies to reading responses, and not writing
  // requests.
  int HandleConnectionClosedBeforeEndOfHeaders();

  // Return true if based on the bytes read so far, the start of the
  // status line is known. This is used to distingish between HTTP/0.9
  // responses (which have no status line) and HTTP/1.x responses.
  bool has_found_status_line_start() const {
    return header_buf_http_offset_ != -1;
  }

  // Sets up the state machine to restart the transaction with auth.
  void PrepareForAuthRestart(HttpAuth::Target target);

  // Called when we don't need to drain the response body or have drained it.
  // Resets |connection_| unless |keep_alive| is true, then calls
  // ResetStateForRestart.  Sets |next_state_| appropriately.
  void DidDrainBodyForAuthRestart(bool keep_alive);

  // Resets the members of the transaction so it can be restarted.
  void ResetStateForRestart();

  // Returns true if we should try to add a Proxy-Authorization header
  bool ShouldApplyProxyAuth() const;

  // Returns true if we should try to add an Authorization header.
  bool ShouldApplyServerAuth() const;

  // Builds either the proxy auth header, or the origin server auth header,
  // as specified by |target|.
  std::string BuildAuthorizationHeader(HttpAuth::Target target) const;

  // Returns a log message for all the response headers related to the auth
  // challenge.
  std::string AuthChallengeLogMessage() const;

  // Handles HTTP status code 401 or 407.
  // HandleAuthChallenge() returns a network error code, or OK on success.
  // May update |pending_auth_target_| or |response_.auth_challenge|.
  int HandleAuthChallenge();

  // Populates response_.auth_challenge with the challenge information, so that
  // URLRequestHttpJob can prompt for a username/password.
  void PopulateAuthChallenge(HttpAuth::Target target);

  // Invalidates any auth cache entries after authentication has failed.
  // The identity that was rejected is auth_identity_[target].
  void InvalidateRejectedAuthFromCache(HttpAuth::Target target);

  // Sets auth_identity_[target] to the next identity that the transaction
  // should try. It chooses candidates by searching the auth cache
  // and the URL for a username:password. Returns true if an identity
  // was found.
  bool SelectNextAuthIdentityToTry(HttpAuth::Target target);

  // Searches the auth cache for an entry that encompasses the request's path.
  // If such an entry is found, updates auth_identity_[target] and
  // auth_handler_[target] with the cache entry's data and returns true.
  bool SelectPreemptiveAuth(HttpAuth::Target target);

  bool HaveAuth(HttpAuth::Target target) const {
    return auth_handler_[target].get() && !auth_identity_[target].invalid;
  }

  // Get the {scheme, host, port} for the authentication target
  GURL AuthOrigin(HttpAuth::Target target) const;

  // Get the absolute path of the resource needing authentication.
  // For proxy authentication the path is always empty string.
  std::string AuthPath(HttpAuth::Target target) const;

  // Returns a string representation of a HttpAuth::Target value that can be
  // used in log messages.
  static std::string AuthTargetString(HttpAuth::Target target);

  // The following three auth members are arrays of size two -- index 0 is
  // for the proxy server, and index 1 is for the origin server.
  // Use the enum HttpAuth::Target to index into them.

  // auth_handler encapsulates the logic for the particular auth-scheme.
  // This includes the challenge's parameters. If NULL, then there is no
  // associated auth handler.
  scoped_refptr<HttpAuthHandler> auth_handler_[2];

  // auth_identity_ holds the (username/password) that should be used by
  // the auth_handler_ to generate credentials. This identity can come from
  // a number of places (url, cache, prompt).
  HttpAuth::Identity auth_identity_[2];

  // Whether this transaction is waiting for proxy auth, server auth, or is
  // not waiting for any auth at all. |pending_auth_target_| is read and
  // cleared by RestartWithAuth().
  HttpAuth::Target pending_auth_target_;

  CompletionCallbackImpl<HttpNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  scoped_refptr<HttpNetworkSession> session_;

  const HttpRequestInfo* request_;
  HttpResponseInfo response_;

  ProxyService::PacRequest* pac_request_;
  ProxyInfo proxy_info_;

  ClientSocketFactory* socket_factory_;
  ClientSocketHandle connection_;
  scoped_ptr<HttpStream> http_stream_;
  bool reused_socket_;

  bool using_ssl_;     // True if handling a HTTPS request
  ProxyMode proxy_mode_;

  // True while establishing a tunnel.  This allows the HTTP CONNECT
  // request/response to reuse the STATE_WRITE_HEADERS,
  // STATE_WRITE_HEADERS_COMPLETE, STATE_READ_HEADERS, and
  // STATE_READ_HEADERS_COMPLETE states and allows us to tell them apart from
  // the real request/response of the transaction.
  bool establishing_tunnel_;

  // Only used between the states
  // STATE_READ_BODY/STATE_DRAIN_BODY_FOR_AUTH and
  // STATE_READ_BODY_COMPLETE/STATE_DRAIN_BODY_FOR_AUTH_COMPLETE.
  //
  // Set to true when DoReadBody or DoDrainBodyForAuthRestart starts to read
  // the response body from the socket, and set to false when the socket read
  // call completes. DoReadBodyComplete and DoDrainBodyForAuthRestartComplete
  // use this boolean to disambiguate a |result| of 0 between a connection
  // closure (EOF) and reaching the end of the response body (no more data).
  //
  // TODO(wtc): this is similar to the |ignore_ok_result_| member of the
  // SSLClientSocketWin class.  We may want to add an internal error code, say
  // ERR_EOF, to indicate a connection closure, so that 0 simply means 0 bytes
  // or OK.  Note that we already have an ERR_CONNECTION_CLOSED error code,
  // but it isn't really being used.
  bool reading_body_from_socket_;

  SSLConfig ssl_config_;

  scoped_refptr<RequestHeaders> request_headers_;
  size_t request_headers_bytes_sent_;
  scoped_ptr<UploadDataStream> request_body_stream_;

  // The read buffer |header_buf_| may be larger than it is full.  The
  // 'capacity' indicates the allocation size of the buffer, and the 'len'
  // indicates how much data is in the buffer already.  The 'body offset'
  // indicates the offset of the start of the response body within the read
  // buffer.
  scoped_refptr<ResponseHeaders> header_buf_;
  int header_buf_capacity_;
  int header_buf_len_;
  int header_buf_body_offset_;

  // The number of bytes by which the header buffer is grown when it reaches
  // capacity.
  enum { kHeaderBufInitialSize = 4096 };

  // |kMaxHeaderBufSize| is the number of bytes that the response headers can
  // grow to. If the body start is not found within this range of the
  // response, the transaction will fail with ERR_RESPONSE_HEADERS_TOO_BIG.
  // Note: |kMaxHeaderBufSize| should be a multiple of |kHeaderBufInitialSize|.
  enum { kMaxHeaderBufSize = 32768 };  // 32 kilobytes.

  // The size in bytes of the buffer we use to drain the response body that
  // we want to throw away.  The response body is typically a small error
  // page just a few hundred bytes long.
  enum { kDrainBodyBufferSize = 1024 };

  // The position where status line starts; -1 if not found yet.
  int header_buf_http_offset_;

  // Indicates the content length remaining to read.  If this value is less
  // than zero (and chunked_decoder_ is null), then we read until the server
  // closes the connection.
  int64 response_body_length_;

  // Keeps track of the number of response body bytes read so far.
  int64 response_body_read_;

  scoped_ptr<HttpChunkedDecoder> chunked_decoder_;

  // User buffer and length passed to the Read method.
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;

  // The time the Start method was called.
  base::Time start_time_;

  // The time the Connect() method was called (if it got called).
  base::Time connect_start_time_;

  // The time the host resolution started (if it indeed got started).
  base::Time host_resolution_start_time_;

  // The next state in the state machine.
  State next_state_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
