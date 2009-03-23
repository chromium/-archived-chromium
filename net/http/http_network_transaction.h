// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
#define NET_HTTP_HTTP_NETWORK_TRANSACTION_H_

#include <string>

#include "base/ref_counted.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/client_socket_handle.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

class ClientSocketFactory;
class HttpChunkedDecoder;
class HttpNetworkSession;
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
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  FRIEND_TEST(HttpNetworkTransactionTest, ResetStateForRestart);

  void BuildRequestHeaders();
  void BuildTunnelRequest();
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
  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoConnect();
  int DoConnectComplete(int result);
  int DoSSLConnectOverTunnel();
  int DoSSLConnectOverTunnelComplete(int result);
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

  // Record histogram of latency (first byte sent till last byte received) as
  // well as effective bandwidth used.
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

  // Called to possibly recover from an SSL handshake error.  Sets next_state_
  // and returns OK if recovering from the error.  Otherwise, the same error
  // code is returned.
  int HandleSSLHandshakeError(int error);

  // Called to possibly recover from the given error.  Sets next_state_ and
  // returns OK if recovering from the error.  Otherwise, the same error code
  // is returned.
  int HandleIOError(int error);

  // Called when we reached EOF or got an error.  If we should resend the
  // request, sets next_state_ and returns true.  Otherwise, does nothing and
  // returns false.
  bool ShouldResendRequest();

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

  // Attach any credentials needed for the proxy server or origin server.
  void ApplyAuth();

  // Helper used by ApplyAuth(). Adds either the proxy auth header, or the
  // origin server auth header, as specified by |target|
  void AddAuthorizationHeader(HttpAuth::Target target);

  // Handles HTTP status code 401 or 407.
  // HandleAuthChallenge() returns a network error code, or OK, or
  // WILL_RESTART_TRANSACTION. The latter indicates that the state machine has
  // been updated to restart the transaction with a new auth attempt.
  enum { WILL_RESTART_TRANSACTION = 1 };
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

  bool NeedAuth(HttpAuth::Target target) const {
    return auth_handler_[target].get() && auth_identity_[target].invalid;
  }

  bool HaveAuth(HttpAuth::Target target) const {
    return auth_handler_[target].get() && !auth_identity_[target].invalid;
  }

  // Get the {scheme, host, port} for the authentication target
  GURL AuthOrigin(HttpAuth::Target target) const;

  // Get the absolute path of the resource needing authentication.
  // For proxy authentication the path is always empty string.
  std::string AuthPath(HttpAuth::Target target) const;

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

  CompletionCallbackImpl<HttpNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  scoped_refptr<HttpNetworkSession> session_;

  const HttpRequestInfo* request_;
  HttpResponseInfo response_;

  ProxyService::PacRequest* pac_request_;
  ProxyInfo proxy_info_;

  HostResolver resolver_;
  AddressList addresses_;

  ClientSocketFactory* socket_factory_;
  ClientSocketHandle connection_;
  bool reused_socket_;

  bool using_ssl_;     // True if handling a HTTPS request
  bool using_proxy_;   // True if using a proxy for HTTP (not HTTPS)
  bool using_tunnel_;  // True if using a tunnel for HTTPS

  // True while establishing a tunnel.  This allows the HTTP CONNECT
  // request/response to reuse the STATE_WRITE_HEADERS,
  // STATE_WRITE_HEADERS_COMPLETE, STATE_READ_HEADERS, and
  // STATE_READ_HEADERS_COMPLETE states and allows us to tell them apart from
  // the real request/response of the transaction.
  bool establishing_tunnel_;

  SSLConfig ssl_config_;

  std::string request_headers_;
  size_t request_headers_bytes_sent_;
  scoped_ptr<UploadDataStream> request_body_stream_;

  // The read buffer may be larger than it is full.  The 'capacity' indicates
  // the allocation size of the buffer, and the 'len' indicates how much data
  // is in the buffer already.  The 'body offset' indicates the offset of the
  // start of the response body within the read buffer.
  scoped_ptr_malloc<char> header_buf_;
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

  enum State {
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,
    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
    STATE_SSL_CONNECT_OVER_TUNNEL,
    STATE_SSL_CONNECT_OVER_TUNNEL_COMPLETE,
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
  State next_state_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
