// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include "base/scoped_ptr.h"
#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "build/build_config.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socks_client_socket.h"
#include "net/socket/ssl_client_socket.h"

using base::Time;

namespace net {

void HttpNetworkTransaction::ResponseHeaders::Realloc(size_t new_size) {
  headers_.reset(static_cast<char*>(realloc(headers_.release(), new_size)));
}

namespace {

void BuildRequestHeaders(const HttpRequestInfo* request_info,
                         const std::string& authorization_headers,
                         const UploadDataStream* upload_data_stream,
                         bool using_proxy,
                         std::string* request_headers) {
  const std::string path = using_proxy ?
      HttpUtil::SpecForRequest(request_info->url) :
      HttpUtil::PathForRequest(request_info->url);
  *request_headers =
      StringPrintf("%s %s HTTP/1.1\r\nHost: %s\r\n",
                   request_info->method.c_str(), path.c_str(),
                   GetHostAndOptionalPort(request_info->url).c_str());

  // For compat with HTTP/1.0 servers and proxies:
  if (using_proxy)
    *request_headers += "Proxy-";
  *request_headers += "Connection: keep-alive\r\n";

  if (!request_info->user_agent.empty()) {
    StringAppendF(request_headers, "User-Agent: %s\r\n",
                  request_info->user_agent.c_str());
  }

  // Our consumer should have made sure that this is a safe referrer.  See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (request_info->referrer.is_valid())
    StringAppendF(request_headers, "Referer: %s\r\n",
                  request_info->referrer.spec().c_str());

  // Add a content length header?
  if (upload_data_stream) {
    StringAppendF(request_headers, "Content-Length: %llu\r\n",
                  upload_data_stream->size());
  } else if (request_info->method == "POST" || request_info->method == "PUT" ||
             request_info->method == "HEAD") {
    // An empty POST/PUT request still needs a content length.  As for HEAD,
    // IE and Safari also add a content length header.  Presumably it is to
    // support sending a HEAD request to an URL that only expects to be sent a
    // POST or some other method that normally would have a message body.
    *request_headers += "Content-Length: 0\r\n";
  }

  // Honor load flags that impact proxy caches.
  if (request_info->load_flags & LOAD_BYPASS_CACHE) {
    *request_headers += "Pragma: no-cache\r\nCache-Control: no-cache\r\n";
  } else if (request_info->load_flags & LOAD_VALIDATE_CACHE) {
    *request_headers += "Cache-Control: max-age=0\r\n";
  }

  if (!authorization_headers.empty()) {
    *request_headers += authorization_headers;
  }

  // TODO(darin): Need to prune out duplicate headers.

  *request_headers += request_info->extra_headers;
  *request_headers += "\r\n";
}

// The HTTP CONNECT method for establishing a tunnel connection is documented
// in draft-luotonen-web-proxy-tunneling-01.txt and RFC 2817, Sections 5.2 and
// 5.3.
void BuildTunnelRequest(const HttpRequestInfo* request_info,
                        const std::string& authorization_headers,
                        std::string* request_headers) {
  // RFC 2616 Section 9 says the Host request-header field MUST accompany all
  // HTTP/1.1 requests.  Add "Proxy-Connection: keep-alive" for compat with
  // HTTP/1.0 proxies such as Squid (required for NTLM authentication).
  *request_headers = StringPrintf(
      "CONNECT %s HTTP/1.1\r\nHost: %s\r\nProxy-Connection: keep-alive\r\n",
      GetHostAndPort(request_info->url).c_str(),
      GetHostAndOptionalPort(request_info->url).c_str());

  if (!request_info->user_agent.empty())
    StringAppendF(request_headers, "User-Agent: %s\r\n",
                  request_info->user_agent.c_str());

  if (!authorization_headers.empty()) {
    *request_headers += authorization_headers;
  }

  *request_headers += "\r\n";
}

}  // namespace

//-----------------------------------------------------------------------------

HttpNetworkTransaction::HttpNetworkTransaction(HttpNetworkSession* session,
                                               ClientSocketFactory* csf)
    : pending_auth_target_(HttpAuth::AUTH_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &HttpNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      session_(session),
      request_(NULL),
      pac_request_(NULL),
      socket_factory_(csf),
      connection_(session->connection_pool()),
      reused_socket_(false),
      using_ssl_(false),
      proxy_mode_(kDirectConnection),
      establishing_tunnel_(false),
      reading_body_from_socket_(false),
      request_headers_(new RequestHeaders()),
      request_headers_bytes_sent_(0),
      header_buf_(new ResponseHeaders()),
      header_buf_capacity_(0),
      header_buf_len_(0),
      header_buf_body_offset_(-1),
      header_buf_http_offset_(-1),
      response_body_length_(-1),  // -1 means unspecified.
      response_body_read_(0),
      read_buf_len_(0),
      next_state_(STATE_NONE) {
#if defined(OS_WIN)
  // TODO(port): Port the SSLConfigService class to Linux and Mac OS X.
  session->ssl_config_service()->GetSSLConfig(&ssl_config_);
#endif
}

int HttpNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback) {
  UpdateConnectionTypeHistograms(CONNECTION_ANY);

  request_ = request_info;
  start_time_ = base::Time::Now();

  next_state_ = STATE_RESOLVE_PROXY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  if (connection_.socket()->IsConnected()) {
    next_state_ = STATE_WRITE_HEADERS;
  } else {
    connection_.socket()->Disconnect();
    connection_.Reset();
    next_state_ = STATE_INIT_CONNECTION;
  }
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithCertificate(
    X509Certificate* client_cert,
    CompletionCallback* callback) {
  ssl_config_.client_cert = client_cert;
  if (client_cert) {
    session_->ssl_client_auth_cache()->Add(GetHostAndPort(request_->url),
                                           client_cert);
  }
  ssl_config_.send_client_cert = true;
  next_state_ = STATE_INIT_CONNECTION;
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  HttpAuth::Target target = pending_auth_target_;
  if (target == HttpAuth::AUTH_NONE) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  pending_auth_target_ = HttpAuth::AUTH_NONE;

  DCHECK(auth_identity_[target].invalid ||
         (username.empty() && password.empty()));

  if (auth_identity_[target].invalid) {
    // Update the username/password.
    auth_identity_[target].source = HttpAuth::IDENT_SRC_EXTERNAL;
    auth_identity_[target].invalid = false;
    auth_identity_[target].username = username;
    auth_identity_[target].password = password;
  }

  PrepareForAuthRestart(target);

  DCHECK(user_callback_ == NULL);
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;

  return rv;
}

void HttpNetworkTransaction::PrepareForAuthRestart(HttpAuth::Target target) {
  DCHECK(HaveAuth(target));
  DCHECK(auth_identity_[target].source != HttpAuth::IDENT_SRC_PATH_LOOKUP);

  // Add the auth entry to the cache before restarting. We don't know whether
  // the identity is valid yet, but if it is valid we want other transactions
  // to know about it. If an entry for (origin, handler->realm()) already
  // exists, we update it.
  //
  // If auth_identity_[target].source is HttpAuth::IDENT_SRC_NONE,
  // auth_identity_[target] contains no identity because identity is not
  // required yet.
  bool has_auth_identity =
      auth_identity_[target].source != HttpAuth::IDENT_SRC_NONE;
  if (has_auth_identity) {
    session_->auth_cache()->Add(AuthOrigin(target), auth_handler_[target],
        auth_identity_[target].username, auth_identity_[target].password,
        AuthPath(target));
  }

  bool keep_alive = false;
  if (response_.headers->IsKeepAlive()) {
    // If there is a response body of known length, we need to drain it first.
    if (response_body_length_ > 0 || chunked_decoder_.get()) {
      next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
      read_buf_ = new IOBuffer(kDrainBodyBufferSize);  // A bit bucket
      read_buf_len_ = kDrainBodyBufferSize;
      return;
    }
    if (response_body_length_ == 0)  // No response body to drain.
      keep_alive = true;
    // response_body_length_ is -1 and we're not using chunked encoding. We
    // don't know the length of the response body, so we can't reuse this
    // connection even though the server says it's keep-alive.
  }

  // If the auth scheme is connection-based but the proxy/server mistakenly
  // marks the connection as non-keep-alive, the auth is going to fail, so log
  // an error message.
  if (!keep_alive && auth_handler_[target]->is_connection_based() &&
      has_auth_identity) {
    LOG(ERROR) << "Can't perform " << auth_handler_[target]->scheme()
               << " auth to the " << AuthTargetString(target) << " "
               << AuthOrigin(target) << " over a non-keep-alive connection";

    HttpVersion http_version = response_.headers->GetHttpVersion();
    LOG(ERROR) << "  HTTP version is " << http_version.major_value() << "."
               << http_version.minor_value();

    std::string header_val;
    void* iter = NULL;
    while (response_.headers->EnumerateHeader(&iter, "connection",
                                              &header_val)) {
      LOG(ERROR) << "  Has header Connection: " << header_val;
    }

    iter = NULL;
    while (response_.headers->EnumerateHeader(&iter, "proxy-connection",
                                              &header_val)) {
      LOG(ERROR) << "  Has header Proxy-Connection: " << header_val;
    }

    // RFC 4559 requires that a proxy indicate its support of NTLM/Negotiate
    // authentication with a "Proxy-Support: Session-Based-Authentication"
    // response header.
    iter = NULL;
    while (response_.headers->EnumerateHeader(&iter, "proxy-support",
                                              &header_val)) {
      LOG(ERROR) << "  Has header Proxy-Support: " << header_val;
    }
  }

  // We don't need to drain the response body, so we act as if we had drained
  // the response body.
  DidDrainBodyForAuthRestart(keep_alive);
}

void HttpNetworkTransaction::DidDrainBodyForAuthRestart(bool keep_alive) {
  if (keep_alive) {
    next_state_ = STATE_WRITE_HEADERS;
    reused_socket_ = true;
  } else {
    next_state_ = STATE_INIT_CONNECTION;
    connection_.socket()->Disconnect();
    connection_.Reset();
  }

  // Reset the other member variables.
  ResetStateForRestart();
}

int HttpNetworkTransaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(response_.headers);
  DCHECK(buf);
  DCHECK(buf_len > 0);

  if (!connection_.is_initialized())
    return 0;  // connection_ has been reset.  Treat like EOF.

  if (establishing_tunnel_) {
    // We're trying to read the body of the response but we're still trying to
    // establish an SSL tunnel through the proxy.  We can't read these bytes
    // when establishing a tunnel because they might be controlled by an active
    // network attacker.  We don't worry about this for HTTP because an active
    // network attacker can already control HTTP sessions.
    // We reach this case when the user cancels a 407 proxy auth prompt.
    // See http://crbug.com/8473
    DCHECK(response_.headers->response_code() == 407);
    LogBlockedTunnelResponse(response_.headers->response_code());
    return ERR_TUNNEL_CONNECTION_FAILED;
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;

  next_state_ = STATE_READ_BODY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

const HttpResponseInfo* HttpNetworkTransaction::GetResponseInfo() const {
  return (response_.headers || response_.ssl_info.cert ||
          response_.cert_request_info) ? &response_ : NULL;
}

LoadState HttpNetworkTransaction::GetLoadState() const {
  // TODO(wtc): Define a new LoadState value for the
  // STATE_INIT_CONNECTION_COMPLETE state, which delays the HTTP request.
  switch (next_state_) {
    case STATE_RESOLVE_PROXY_COMPLETE:
      return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
    case STATE_INIT_CONNECTION_COMPLETE:
      return connection_.GetLoadState();
    case STATE_WRITE_HEADERS_COMPLETE:
    case STATE_WRITE_BODY_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    case STATE_READ_HEADERS_COMPLETE:
      return LOAD_STATE_WAITING_FOR_RESPONSE;
    case STATE_READ_BODY_COMPLETE:
      return LOAD_STATE_READING_RESPONSE;
    default:
      return LOAD_STATE_IDLE;
  }
}

uint64 HttpNetworkTransaction::GetUploadProgress() const {
  if (!request_body_stream_.get())
    return 0;

  return request_body_stream_->position();
}

HttpNetworkTransaction::~HttpNetworkTransaction() {
  // If we still have an open socket, then make sure to disconnect it so it
  // won't call us back and we don't try to reuse it later on.
  if (connection_.is_initialized())
    connection_.socket()->Disconnect();

  if (pac_request_)
    session_->proxy_service()->CancelPacRequest(pac_request_);
}

void HttpNetworkTransaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // Since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void HttpNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int HttpNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_PROXY:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.resolve_proxy", request_, request_->url.spec());
        rv = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        rv = DoResolveProxyComplete(rv);
        TRACE_EVENT_END("http.resolve_proxy", request_, request_->url.spec());
        break;
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.init_conn", request_, request_->url.spec());
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        TRACE_EVENT_END("http.init_conn", request_, request_->url.spec());
        break;
      case STATE_SOCKS_CONNECT:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.socks_connect", request_, request_->url.spec());
        rv = DoSOCKSConnect();
        break;
      case STATE_SOCKS_CONNECT_COMPLETE:
        rv = DoSOCKSConnectComplete(rv);
        TRACE_EVENT_END("http.socks_connect", request_, request_->url.spec());
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.ssl_connect", request_, request_->url.spec());
        rv = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        rv = DoSSLConnectComplete(rv);
        TRACE_EVENT_END("http.ssl_connect", request_, request_->url.spec());
        break;
      case STATE_WRITE_HEADERS:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.write_headers", request_, request_->url.spec());
        rv = DoWriteHeaders();
        break;
      case STATE_WRITE_HEADERS_COMPLETE:
        rv = DoWriteHeadersComplete(rv);
        TRACE_EVENT_END("http.write_headers", request_, request_->url.spec());
        break;
      case STATE_WRITE_BODY:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.write_body", request_, request_->url.spec());
        rv = DoWriteBody();
        break;
      case STATE_WRITE_BODY_COMPLETE:
        rv = DoWriteBodyComplete(rv);
        TRACE_EVENT_END("http.write_body", request_, request_->url.spec());
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.read_headers", request_, request_->url.spec());
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        TRACE_EVENT_END("http.read_headers", request_, request_->url.spec());
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.read_body", request_, request_->url.spec());
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        TRACE_EVENT_END("http.read_body", request_, request_->url.spec());
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART:
        DCHECK_EQ(OK, rv);
        TRACE_EVENT_BEGIN("http.drain_body_for_auth_restart",
                          request_, request_->url.spec());
        rv = DoDrainBodyForAuthRestart();
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE:
        rv = DoDrainBodyForAuthRestartComplete(rv);
        TRACE_EVENT_END("http.drain_body_for_auth_restart",
                        request_, request_->url.spec());
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpNetworkTransaction::DoResolveProxy() {
  DCHECK(!pac_request_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  if (request_->load_flags & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
    return OK;
  }

  return session_->proxy_service()->ResolveProxy(
      request_->url, &proxy_info_, &io_callback_, &pac_request_);
}

int HttpNetworkTransaction::DoResolveProxyComplete(int result) {
  next_state_ = STATE_INIT_CONNECTION;

  // Remove unsupported proxies (like SOCKS5) from the list.
  proxy_info_.RemoveProxiesWithoutScheme(
      ProxyServer::SCHEME_DIRECT | ProxyServer::SCHEME_HTTP |
      ProxyServer::SCHEME_SOCKS4);

  pac_request_ = NULL;

  if (result != OK) {
    DLOG(ERROR) << "Failed to resolve proxy: " << result;
    proxy_info_.UseDirect();
  }
  return OK;
}

int HttpNetworkTransaction::DoInitConnection() {
  DCHECK(!connection_.is_initialized());

  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  using_ssl_ = request_->url.SchemeIs("https");

  if (proxy_info_.is_direct())
    proxy_mode_ = kDirectConnection;
  else if (proxy_info_.proxy_server().is_socks())
    proxy_mode_ = kSOCKSProxy;
  else if (using_ssl_)
    proxy_mode_ = kHTTPProxyUsingTunnel;
  else
    proxy_mode_ = kHTTPProxy;

  // Build the string used to uniquely identify connections of this type.
  // Determine the host and port to connect to.
  std::string connection_group;
  std::string host;
  int port;
  if (proxy_mode_ != kDirectConnection) {
    ProxyServer proxy_server = proxy_info_.proxy_server();
    connection_group = "proxy/" + proxy_server.ToURI() + "/";
    host = proxy_server.HostNoBrackets();
    port = proxy_server.port();
  } else {
    host = request_->url.HostNoBrackets();
    port = request_->url.EffectiveIntPort();
  }

  // For a connection via HTTP proxy not using CONNECT, the connection
  // is to the proxy server only. For all other cases
  // (direct, HTTP proxy CONNECT, SOCKS), the connection is upto the
  // url endpoint. Hence we append the url data into the connection_group.
  if (proxy_mode_ != kHTTPProxy)
    connection_group.append(request_->url.GetOrigin().spec());

  DCHECK(!connection_group.empty());

  HostResolver::RequestInfo resolve_info(host, port);

  // The referrer is used by the DNS prefetch system to corellate resolutions
  // with the page that triggered them. It doesn't impact the actual addresses
  // that we resolve to.
  resolve_info.set_referrer(request_->referrer);

  // If the user is refreshing the page, bypass the host cache.
  if (request_->load_flags & LOAD_BYPASS_CACHE ||
      request_->load_flags & LOAD_DISABLE_CACHE) {
    resolve_info.set_allow_cached_response(false);
  }

  int rv = connection_.Init(connection_group, resolve_info, request_->priority,
                            &io_callback_);
  return rv;
}

int HttpNetworkTransaction::DoInitConnectionComplete(int result) {
  if (result < 0)
    return ReconsiderProxyAfterError(result);

  DCHECK(connection_.is_initialized());

  // Set the reused_socket_ flag to indicate that we are using a keep-alive
  // connection.  This flag is used to handle errors that occur while we are
  // trying to reuse a keep-alive connection.
  reused_socket_ = connection_.is_reused();
  if (reused_socket_) {
    next_state_ = STATE_WRITE_HEADERS;
  } else {
    // Now we have a TCP connected socket.  Perform other connection setup as
    // needed.
    LogTCPConnectedMetrics();
    if (proxy_mode_ == kSOCKSProxy)
      next_state_ = STATE_SOCKS_CONNECT;
    else if (using_ssl_ && proxy_mode_ == kDirectConnection) {
      next_state_ = STATE_SSL_CONNECT;
    } else {
      next_state_ = STATE_WRITE_HEADERS;
      if (proxy_mode_ == kHTTPProxyUsingTunnel)
        establishing_tunnel_ = true;
    }
  }
  http_stream_.reset(new HttpBasicStream(&connection_));
  return OK;
}

int HttpNetworkTransaction::DoSOCKSConnect() {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;

  // Add a SOCKS connection on top of our existing transport socket.
  ClientSocket* s = connection_.release_socket();
  HostResolver::RequestInfo req_info(request_->url.HostNoBrackets(),
                                     request_->url.EffectiveIntPort());
  req_info.set_referrer(request_->referrer);

  s = new SOCKSClientSocket(s, req_info, session_->host_resolver());
  connection_.set_socket(s);
  return connection_.socket()->Connect(&io_callback_);
}

int HttpNetworkTransaction::DoSOCKSConnectComplete(int result) {
  DCHECK_EQ(kSOCKSProxy, proxy_mode_);

  if (result == OK) {
    if (using_ssl_) {
      next_state_ = STATE_SSL_CONNECT;
    } else {
      next_state_ = STATE_WRITE_HEADERS;
    }
  } else {
    result = ReconsiderProxyAfterError(result);
  }
  return result;
}

int HttpNetworkTransaction::DoSSLConnect() {
  next_state_ = STATE_SSL_CONNECT_COMPLETE;

  if (request_->load_flags & LOAD_VERIFY_EV_CERT)
    ssl_config_.verify_ev_cert = true;

  // Add a SSL socket on top of our existing transport socket.
  ClientSocket* s = connection_.release_socket();
  s = socket_factory_->CreateSSLClientSocket(
      s, request_->url.HostNoBrackets(), ssl_config_);
  connection_.set_socket(s);
  return connection_.socket()->Connect(&io_callback_);
}

int HttpNetworkTransaction::DoSSLConnectComplete(int result) {
  if (IsCertificateError(result))
    result = HandleCertificateError(result);

  if (result == OK) {
    next_state_ = STATE_WRITE_HEADERS;
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    result = HandleCertificateRequest(result);
  } else {
    result = HandleSSLHandshakeError(result);
  }
  return result;
}

int HttpNetworkTransaction::DoWriteHeaders() {
  next_state_ = STATE_WRITE_HEADERS_COMPLETE;

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_headers_->headers_.empty()) {
    // Figure out if we can/should add Proxy-Authentication & Authentication
    // headers.
    bool have_proxy_auth =
        ShouldApplyProxyAuth() &&
        (HaveAuth(HttpAuth::AUTH_PROXY) ||
         SelectPreemptiveAuth(HttpAuth::AUTH_PROXY));
    bool have_server_auth =
        ShouldApplyServerAuth() &&
        (HaveAuth(HttpAuth::AUTH_SERVER) ||
         SelectPreemptiveAuth(HttpAuth::AUTH_SERVER));

    std::string authorization_headers;

    if (have_proxy_auth)
      authorization_headers.append(
          BuildAuthorizationHeader(HttpAuth::AUTH_PROXY));
    if (have_server_auth)
      authorization_headers.append(
          BuildAuthorizationHeader(HttpAuth::AUTH_SERVER));

    if (establishing_tunnel_) {
      BuildTunnelRequest(request_, authorization_headers,
                         &request_headers_->headers_);
    } else {
      if (request_->upload_data)
        request_body_stream_.reset(new UploadDataStream(request_->upload_data));
      BuildRequestHeaders(request_, authorization_headers,
                          request_body_stream_.get(),
                          proxy_mode_ == kHTTPProxy,
                          &request_headers_->headers_);
    }
  }

  // Record our best estimate of the 'request time' as the time when we send
  // out the first bytes of the request headers.
  if (request_headers_bytes_sent_ == 0) {
    response_.request_time = Time::Now();
  }

  request_headers_->SetDataOffset(request_headers_bytes_sent_);
  int buf_len = static_cast<int>(request_headers_->headers_.size() -
                                 request_headers_bytes_sent_);
  DCHECK_GT(buf_len, 0);

  return http_stream_->Write(request_headers_, buf_len, &io_callback_);
}

int HttpNetworkTransaction::DoWriteHeadersComplete(int result) {
  if (result < 0)
    return HandleIOError(result);

  request_headers_bytes_sent_ += result;
  if (request_headers_bytes_sent_ < request_headers_->headers_.size()) {
    next_state_ = STATE_WRITE_HEADERS;
  } else if (!establishing_tunnel_ && request_body_stream_.get() &&
             request_body_stream_->size()) {
    next_state_ = STATE_WRITE_BODY;
  } else {
    next_state_ = STATE_READ_HEADERS;
  }
  return OK;
}

int HttpNetworkTransaction::DoWriteBody() {
  next_state_ = STATE_WRITE_BODY_COMPLETE;

  DCHECK(request_body_stream_.get());
  DCHECK(request_body_stream_->size());

  int buf_len = static_cast<int>(request_body_stream_->buf_len());

  return http_stream_->Write(request_body_stream_->buf(), buf_len,
                             &io_callback_);
}

int HttpNetworkTransaction::DoWriteBodyComplete(int result) {
  if (result < 0)
    return HandleIOError(result);

  request_body_stream_->DidConsume(result);

  if (request_body_stream_->position() < request_body_stream_->size()) {
    next_state_ = STATE_WRITE_BODY;
  } else {
    next_state_ = STATE_READ_HEADERS;
  }
  return OK;
}

int HttpNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;

  // Grow the read buffer if necessary.
  if (header_buf_len_ == header_buf_capacity_) {
    header_buf_capacity_ += kHeaderBufInitialSize;
    header_buf_->Realloc(header_buf_capacity_);
  }

  int buf_len = header_buf_capacity_ - header_buf_len_;
  header_buf_->set_data(header_buf_len_);

  return http_stream_->Read(header_buf_, buf_len, &io_callback_);
}

int HttpNetworkTransaction::HandleConnectionClosedBeforeEndOfHeaders() {
  if (establishing_tunnel_) {
    // The connection was closed before the tunnel could be established.
    return ERR_TUNNEL_CONNECTION_FAILED;
  }

  if (has_found_status_line_start()) {
    // Assume EOF is end-of-headers.
    header_buf_body_offset_ = header_buf_len_;
    return OK;
  }

  // No status line was matched yet. Could have been a HTTP/0.9 response, or
  // a partial HTTP/1.x response.

  if (header_buf_len_ == 0) {
    // The connection was closed before any data was sent. Likely an error
    // rather than empty HTTP/0.9 response.
    return ERR_EMPTY_RESPONSE;
  }

  // Assume everything else is a HTTP/0.9 response (including responses
  // of 'h', 'ht', 'htt').
  header_buf_body_offset_ = 0;
  return OK;
}

int HttpNetworkTransaction::DoReadHeadersComplete(int result) {
  // We can get a certificate error or ERR_SSL_CLIENT_AUTH_CERT_NEEDED here
  // due to SSL renegotiation.
  if (using_ssl_) {
    if (IsCertificateError(result)) {
      // We don't handle a certificate error during SSL renegotiation, so we
      // have to return an error that's not in the certificate error range
      // (-2xx).
      LOG(ERROR) << "Got a server certificate with error " << result
                 << " during SSL renegotiation";
      result = ERR_CERT_ERROR_IN_SSL_RENEGOTIATION;
    } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
      result = HandleCertificateRequest(result);
      if (result == OK)
        return result;
    }
  }

  if (result < 0)
    return HandleIOError(result);

  if (result == 0 && ShouldResendRequest()) {
    ResetConnectionAndRequestForResend();
    return result;
  }

  // Record our best estimate of the 'response time' as the time when we read
  // the first bytes of the response headers.
  if (header_buf_len_ == 0) {
    // After we call RestartWithAuth header_buf_len will be zero again, and
    // we need to be cautious about incorrectly logging the duration across the
    // authentication activitiy.
    bool first_response = response_.response_time == Time();
    response_.response_time = Time::Now();
    if (first_response)
      LogTransactionConnectedMetrics();
  }

  // The socket was closed before we found end-of-headers.
  if (result == 0) {
    int rv = HandleConnectionClosedBeforeEndOfHeaders();
    if (rv != OK)
      return rv;
  } else {
    header_buf_len_ += result;
    DCHECK(header_buf_len_ <= header_buf_capacity_);

    // Look for the start of the status line, if it hasn't been found yet.
    if (!has_found_status_line_start()) {
      header_buf_http_offset_ = HttpUtil::LocateStartOfStatusLine(
          header_buf_->headers(), header_buf_len_);
    }

    if (has_found_status_line_start()) {
      int eoh = HttpUtil::LocateEndOfHeaders(
          header_buf_->headers(), header_buf_len_, header_buf_http_offset_);
      if (eoh == -1) {
        // Prevent growing the headers buffer indefinitely.
        if (header_buf_len_ >= kMaxHeaderBufSize)
          return ERR_RESPONSE_HEADERS_TOO_BIG;

        // Haven't found the end of headers yet, keep reading.
        next_state_ = STATE_READ_HEADERS;
        return OK;
      }
      header_buf_body_offset_ = eoh;
    } else if (header_buf_len_ < 8) {
      // Not enough data to decide whether this is HTTP/0.9 yet.
      // 8 bytes = (4 bytes of junk) + "http".length()
      next_state_ = STATE_READ_HEADERS;
      return OK;
    } else {
      // Enough data was read -- there is no status line.
      header_buf_body_offset_ = 0;
    }
  }

  // And, we are done with the Start or the SSL tunnel CONNECT sequence.
  return DidReadResponseHeaders();
}

int HttpNetworkTransaction::DoReadBody() {
  DCHECK(read_buf_);
  DCHECK_GT(read_buf_len_, 0);
  DCHECK(connection_.is_initialized());
  DCHECK(!header_buf_->headers() || header_buf_body_offset_ >= 0);

  next_state_ = STATE_READ_BODY_COMPLETE;

  // We may have already consumed the indicated content length.
  if (response_body_length_ != -1 &&
      response_body_read_ >= response_body_length_)
    return 0;

  // We may have some data remaining in the header buffer.
  if (header_buf_->headers() && header_buf_body_offset_ < header_buf_len_) {
    int n = std::min(read_buf_len_, header_buf_len_ - header_buf_body_offset_);
    memcpy(read_buf_->data(), header_buf_->headers() + header_buf_body_offset_,
           n);
    header_buf_body_offset_ += n;
    if (header_buf_body_offset_ == header_buf_len_) {
      header_buf_->Reset();
      header_buf_capacity_ = 0;
      header_buf_len_ = 0;
      header_buf_body_offset_ = -1;
    }
    return n;
  }

  reading_body_from_socket_ = true;
  return http_stream_->Read(read_buf_, read_buf_len_, &io_callback_);
}

int HttpNetworkTransaction::DoReadBodyComplete(int result) {
  // We are done with the Read call.
  DCHECK(!establishing_tunnel_) <<
      "We should never read a response body of a tunnel.";

  bool unfiltered_eof = (result == 0 && reading_body_from_socket_);
  reading_body_from_socket_ = false;

  // Filter incoming data if appropriate.  FilterBuf may return an error.
  if (result > 0 && chunked_decoder_.get()) {
    result = chunked_decoder_->FilterBuf(read_buf_->data(), result);
    if (result == 0 && !chunked_decoder_->reached_eof()) {
      // Don't signal completion of the Read call yet or else it'll look like
      // we received end-of-file.  Wait for more data.
      next_state_ = STATE_READ_BODY;
      return OK;
    }
  }

  bool done = false, keep_alive = false;
  if (result < 0) {
    // Error while reading the socket.
    done = true;
  } else {
    response_body_read_ += result;
    if (unfiltered_eof ||
        (response_body_length_ != -1 &&
         response_body_read_ >= response_body_length_) ||
        (chunked_decoder_.get() && chunked_decoder_->reached_eof())) {
      done = true;
      keep_alive = response_.headers->IsKeepAlive();
      // We can't reuse the connection if we read more than the advertised
      // content length.
      if (unfiltered_eof ||
          (response_body_length_ != -1 &&
           response_body_read_ > response_body_length_))
        keep_alive = false;
    }
  }

  // Clean up connection_ if we are done.
  if (done) {
    LogTransactionMetrics();
    if (!keep_alive)
      connection_.socket()->Disconnect();
    connection_.Reset();
    // The next Read call will return 0 (EOF).
  }

  // Clear these to avoid leaving around old state.
  read_buf_ = NULL;
  read_buf_len_ = 0;

  return result;
}

int HttpNetworkTransaction::DoDrainBodyForAuthRestart() {
  // This method differs from DoReadBody only in the next_state_.  So we just
  // call DoReadBody and override the next_state_.  Perhaps there is a more
  // elegant way for these two methods to share code.
  int rv = DoReadBody();
  DCHECK(next_state_ == STATE_READ_BODY_COMPLETE);
  next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE;
  return rv;
}

// TODO(wtc): The first two thirds of this method and the DoReadBodyComplete
// method are almost the same.  Figure out a good way for these two methods
// to share code.
int HttpNetworkTransaction::DoDrainBodyForAuthRestartComplete(int result) {
  bool unfiltered_eof = (result == 0 && reading_body_from_socket_);
  reading_body_from_socket_ = false;

  // Filter incoming data if appropriate.  FilterBuf may return an error.
  if (result > 0 && chunked_decoder_.get()) {
    result = chunked_decoder_->FilterBuf(read_buf_->data(), result);
    if (result == 0 && !chunked_decoder_->reached_eof()) {
      // Don't signal completion of the Read call yet or else it'll look like
      // we received end-of-file.  Wait for more data.
      next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
      return OK;
    }
  }

  // keep_alive defaults to true because the very reason we're draining the
  // response body is to reuse the connection for auth restart.
  bool done = false, keep_alive = true;
  if (result < 0) {
    // Error while reading the socket.
    done = true;
    keep_alive = false;
  } else {
    response_body_read_ += result;
    if (unfiltered_eof ||
        (response_body_length_ != -1 &&
         response_body_read_ >= response_body_length_) ||
        (chunked_decoder_.get() && chunked_decoder_->reached_eof())) {
      done = true;
      // We can't reuse the connection if we read more than the advertised
      // content length.
      if (unfiltered_eof ||
          (response_body_length_ != -1 &&
           response_body_read_ > response_body_length_))
        keep_alive = false;
    }
  }

  if (done) {
    DidDrainBodyForAuthRestart(keep_alive);
  } else {
    // Keep draining.
    next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
  }

  return OK;
}

void HttpNetworkTransaction::LogTCPConnectedMetrics() const {
  base::TimeDelta host_resolution_and_tcp_connection_latency =
      base::Time::Now() - host_resolution_start_time_;

  UMA_HISTOGRAM_CLIPPED_TIMES(
      "Net.Dns_Resolution_And_TCP_Connection_Latency",
      host_resolution_and_tcp_connection_latency,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);

  UMA_HISTOGRAM_COUNTS_100(
      "Net.TCP_Connection_Idle_Sockets",
      session_->connection_pool()->IdleSocketCountInGroup(
          connection_.group_name()));
}

void HttpNetworkTransaction::LogTransactionConnectedMetrics() const {
  base::TimeDelta total_duration = response_.response_time - start_time_;

  UMA_HISTOGRAM_CLIPPED_TIMES(
      "Net.Transaction_Connected_Under_10",
      total_duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);
  if (!reused_socket_)
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Transaction_Connected_New",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);

  // Currently, non-zero priority requests are frame or sub-frame resource
  // types.  This will change when we also prioritize certain subresources like
  // css, js, etc.
  if (request_->priority) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Priority_High_Latency",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  } else {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Priority_Low_Latency",
        total_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }
}

void HttpNetworkTransaction::LogTransactionMetrics() const {
  base::TimeDelta duration = base::Time::Now() - response_.request_time;
  if (60 < duration.InMinutes())
    return;

  base::TimeDelta total_duration = base::Time::Now() - start_time_;

  UMA_HISTOGRAM_LONG_TIMES("Net.Transaction_Latency", duration);
  UMA_HISTOGRAM_CLIPPED_TIMES("Net.Transaction_Latency_Under_10", duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
      100);
  UMA_HISTOGRAM_CLIPPED_TIMES("Net.Transaction_Latency_Total_Under_10",
      total_duration, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(10), 100);
  if (!reused_socket_) {
    UMA_HISTOGRAM_CLIPPED_TIMES(
        "Net.Transaction_Latency_Total_New_Connection_Under_10",
        total_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10), 100);
  }
}

void HttpNetworkTransaction::LogBlockedTunnelResponse(
    int response_code) const {
  LOG(WARNING) << "Blocked proxy response with status " << response_code
               << " to CONNECT request for "
               << GetHostAndPort(request_->url) << ".";
}

int HttpNetworkTransaction::DidReadResponseHeaders() {
  DCHECK_GE(header_buf_body_offset_, 0);

  scoped_refptr<HttpResponseHeaders> headers;
  if (has_found_status_line_start()) {
    headers = new HttpResponseHeaders(
        HttpUtil::AssembleRawHeaders(
            header_buf_->headers(), header_buf_body_offset_));
  } else {
    // Fabricate a status line to to preserve the HTTP/0.9 version.
    // (otherwise HttpResponseHeaders will default it to HTTP/1.0).
    headers = new HttpResponseHeaders(std::string("HTTP/0.9 200 OK"));
  }

  if (headers->GetParsedHttpVersion() < HttpVersion(1, 0)) {
    // Require the "HTTP/1.x" status line for SSL CONNECT.
    if (establishing_tunnel_)
      return ERR_TUNNEL_CONNECTION_FAILED;

    // HTTP/0.9 doesn't support the PUT method, so lack of response headers
    // indicates a buggy server.  See:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=193921
    if (request_->method == "PUT")
      return ERR_METHOD_NOT_SUPPORTED;
  }

  if (establishing_tunnel_) {
    switch (headers->response_code()) {
      case 200:  // OK
        if (header_buf_body_offset_ != header_buf_len_) {
          // The proxy sent extraneous data after the headers.
          return ERR_TUNNEL_CONNECTION_FAILED;
        }
        next_state_ = STATE_SSL_CONNECT;
        // Reset for the real request and response headers.
        request_headers_->headers_.clear();
        request_headers_bytes_sent_ = 0;
        header_buf_len_ = 0;
        header_buf_body_offset_ = -1;
        establishing_tunnel_ = false;
        return OK;

      // We aren't able to CONNECT to the remote host through the proxy.  We
      // need to be very suspicious about the response because an active network
      // attacker can force us into this state by masquerading as the proxy.
      // The only safe thing to do here is to fail the connection because our
      // client is expecting an SSL protected response.
      // See http://crbug.com/7338.
      case 407:  // Proxy Authentication Required
        // We need this status code to allow proxy authentication.  Our
        // authentication code is smart enough to avoid being tricked by an
        // active network attacker.
        break;
      default:
        // For all other status codes, we conservatively fail the CONNECT
        // request.
        // We lose something by doing this.  We have seen proxy 403, 404, and
        // 501 response bodies that contain a useful error message.  For
        // example, Squid uses a 404 response to report the DNS error: "The
        // domain name does not exist."
        LogBlockedTunnelResponse(headers->response_code());
        return ERR_TUNNEL_CONNECTION_FAILED;
    }
  }

  // Check for an intermediate 100 Continue response.  An origin server is
  // allowed to send this response even if we didn't ask for it, so we just
  // need to skip over it.
  // We treat any other 1xx in this same way (although in practice getting
  // a 1xx that isn't a 100 is rare).
  if (headers->response_code() / 100 == 1) {
    header_buf_len_ -= header_buf_body_offset_;
    // If we've already received some bytes after the 1xx response,
    // move them to the beginning of header_buf_.
    if (header_buf_len_) {
      memmove(header_buf_->headers(),
              header_buf_->headers() + header_buf_body_offset_,
              header_buf_len_);
    }
    header_buf_body_offset_ = -1;
    next_state_ = STATE_READ_HEADERS;
    return OK;
  }

  response_.headers = headers;
  response_.vary_data.Init(*request_, *response_.headers);

  // Figure how to determine EOF:

  // For certain responses, we know the content length is always 0. From
  // RFC 2616 Section 4.3 Message Body:
  //
  // For response messages, whether or not a message-body is included with
  // a message is dependent on both the request method and the response
  // status code (section 6.1.1). All responses to the HEAD request method
  // MUST NOT include a message-body, even though the presence of entity-
  // header fields might lead one to believe they do. All 1xx
  // (informational), 204 (no content), and 304 (not modified) responses
  // MUST NOT include a message-body. All other responses do include a
  // message-body, although it MAY be of zero length.
  switch (response_.headers->response_code()) {
    // Note that 1xx was already handled earlier.
    case 204:  // No Content
    case 205:  // Reset Content
    case 304:  // Not Modified
      response_body_length_ = 0;
      break;
  }
  if (request_->method == "HEAD")
    response_body_length_ = 0;

  if (response_body_length_ == -1) {
    // Ignore spurious chunked responses from HTTP/1.0 servers and proxies.
    // Otherwise "Transfer-Encoding: chunked" trumps "Content-Length: N"
    if (response_.headers->GetHttpVersion() >= HttpVersion(1, 1) &&
        response_.headers->HasHeaderValue("Transfer-Encoding", "chunked")) {
      chunked_decoder_.reset(new HttpChunkedDecoder());
    } else {
      response_body_length_ = response_.headers->GetContentLength();
      // If response_body_length_ is still -1, then we have to wait for the
      // server to close the connection.
    }
  }

  int rv = HandleAuthChallenge();
  if (rv != OK)
    return rv;

  if (using_ssl_ && !establishing_tunnel_) {
    SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(connection_.socket());
    ssl_socket->GetSSLInfo(&response_.ssl_info);
  }

  return OK;
}

int HttpNetworkTransaction::HandleCertificateError(int error) {
  DCHECK(using_ssl_);

  const int kCertFlags = LOAD_IGNORE_CERT_COMMON_NAME_INVALID |
                         LOAD_IGNORE_CERT_DATE_INVALID |
                         LOAD_IGNORE_CERT_AUTHORITY_INVALID |
                         LOAD_IGNORE_CERT_WRONG_USAGE;
  if (request_->load_flags & kCertFlags) {
    switch (error) {
      case ERR_CERT_COMMON_NAME_INVALID:
        if (request_->load_flags & LOAD_IGNORE_CERT_COMMON_NAME_INVALID)
          error = OK;
        break;
      case ERR_CERT_DATE_INVALID:
        if (request_->load_flags & LOAD_IGNORE_CERT_DATE_INVALID)
          error = OK;
        break;
      case ERR_CERT_AUTHORITY_INVALID:
        if (request_->load_flags & LOAD_IGNORE_CERT_AUTHORITY_INVALID)
          error = OK;
        break;
    }
  }

  if (error != OK) {
    SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(connection_.socket());
    ssl_socket->GetSSLInfo(&response_.ssl_info);

    // Add the bad certificate to the set of allowed certificates in the
    // SSL info object. This data structure will be consulted after calling
    // RestartIgnoringLastError(). And the user will be asked interactively
    // before RestartIgnoringLastError() is ever called.
    ssl_config_.allowed_bad_certs_.insert(response_.ssl_info.cert);
  }
  return error;
}

int HttpNetworkTransaction::HandleCertificateRequest(int error) {
  // Assert that the socket did not send a client certificate.
  // Note: If we got a reused socket, it was created with some other
  // transaction's ssl_config_, so we need to disable this assertion.  We can
  // get a certificate request on a reused socket when the server requested
  // renegotiation (rehandshake).
  // TODO(wtc): add a GetSSLParams method to SSLClientSocket so we can query
  // the SSL parameters it was created with and get rid of the reused_socket_
  // test.
  DCHECK(reused_socket_ || !ssl_config_.send_client_cert);

  response_.cert_request_info = new SSLCertRequestInfo;
  SSLClientSocket* ssl_socket =
      reinterpret_cast<SSLClientSocket*>(connection_.socket());
  ssl_socket->GetSSLCertRequestInfo(response_.cert_request_info);

  // Close the connection while the user is selecting a certificate to send
  // to the server.
  connection_.socket()->Disconnect();
  connection_.Reset();

  // If the user selected one of the certificate in client_certs for this
  // server before, use it automatically.
  X509Certificate* client_cert = session_->ssl_client_auth_cache()->
      Lookup(GetHostAndPort(request_->url));
  if (client_cert) {
    const std::vector<scoped_refptr<X509Certificate> >& client_certs =
        response_.cert_request_info->client_certs;
    for (size_t i = 0; i < client_certs.size(); ++i) {
      if (client_cert->fingerprint().Equals(client_certs[i]->fingerprint())) {
        ssl_config_.client_cert = client_cert;
        ssl_config_.send_client_cert = true;
        next_state_ = STATE_INIT_CONNECTION;
        // Reset the other member variables.
        // Note: this is necessary only with SSL renegotiation.
        ResetStateForRestart();
        return OK;
      }
    }
  }
  return error;
}

int HttpNetworkTransaction::HandleSSLHandshakeError(int error) {
  if (ssl_config_.send_client_cert &&
     (error == ERR_SSL_PROTOCOL_ERROR ||
      error == ERR_BAD_SSL_CLIENT_AUTH_CERT)) {
    session_->ssl_client_auth_cache()->Remove(GetHostAndPort(request_->url));
  }

  switch (error) {
    case ERR_SSL_PROTOCOL_ERROR:
    case ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
      if (ssl_config_.tls1_enabled) {
        // This could be a TLS-intolerant server or an SSL 3.0 server that
        // chose a TLS-only cipher suite.  Turn off TLS 1.0 and retry.
        ssl_config_.tls1_enabled = false;
        connection_.socket()->Disconnect();
        connection_.Reset();
        next_state_ = STATE_INIT_CONNECTION;
        error = OK;
      }
      break;
  }
  return error;
}

// This method determines whether it is safe to resend the request after an
// IO error.  It can only be called in response to request header or body
// write errors or response header read errors.  It should not be used in
// other cases, such as a Connect error.
int HttpNetworkTransaction::HandleIOError(int error) {
  switch (error) {
    // If we try to reuse a connection that the server is in the process of
    // closing, we may end up successfully writing out our request (or a
    // portion of our request) only to find a connection error when we try to
    // read from (or finish writing to) the socket.
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_ABORTED:
      if (ShouldResendRequest()) {
        ResetConnectionAndRequestForResend();
        error = OK;
      }
      break;
  }
  return error;
}

void HttpNetworkTransaction::ResetStateForRestart() {
  pending_auth_target_ = HttpAuth::AUTH_NONE;
  header_buf_->Reset();
  header_buf_capacity_ = 0;
  header_buf_len_ = 0;
  header_buf_body_offset_ = -1;
  header_buf_http_offset_ = -1;
  response_body_length_ = -1;
  response_body_read_ = 0;
  read_buf_ = NULL;
  read_buf_len_ = 0;
  request_headers_->headers_.clear();
  request_headers_bytes_sent_ = 0;
  chunked_decoder_.reset();
  // Reset all the members of response_.
  response_ = HttpResponseInfo();
}

bool HttpNetworkTransaction::ShouldResendRequest() const {
  // NOTE: we resend a request only if we reused a keep-alive connection.
  // This automatically prevents an infinite resend loop because we'll run
  // out of the cached keep-alive connections eventually.
  if (establishing_tunnel_ ||
      !reused_socket_ ||  // We didn't reuse a keep-alive connection.
      header_buf_len_) {  // We have received some response headers.
    return false;
  }
  return true;
}

void HttpNetworkTransaction::ResetConnectionAndRequestForResend() {
  connection_.socket()->Disconnect();
  connection_.Reset();
  // There are two reasons we need to clear request_headers_.  1) It contains
  // the real request headers, but we may need to resend the CONNECT request
  // first to recreate the SSL tunnel.  2) An empty request_headers_ causes
  // BuildRequestHeaders to be called, which rewinds request_body_stream_ to
  // the beginning of request_->upload_data.
  request_headers_->headers_.clear();
  request_headers_bytes_sent_ = 0;
  next_state_ = STATE_INIT_CONNECTION;  // Resend the request.
}

int HttpNetworkTransaction::ReconsiderProxyAfterError(int error) {
  DCHECK(!pac_request_);

  // A failure to resolve the hostname or any error related to establishing a
  // TCP connection could be grounds for trying a new proxy configuration.
  //
  // Why do this when a hostname cannot be resolved?  Some URLs only make sense
  // to proxy servers.  The hostname in those URLs might fail to resolve if we
  // are still using a non-proxy config.  We need to check if a proxy config
  // now exists that corresponds to a proxy server that could load the URL.
  //
  switch (error) {
    case ERR_NAME_NOT_RESOLVED:
    case ERR_INTERNET_DISCONNECTED:
    case ERR_ADDRESS_UNREACHABLE:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_REFUSED:
    case ERR_CONNECTION_ABORTED:
    case ERR_TIMED_OUT:
    case ERR_TUNNEL_CONNECTION_FAILED:
      break;
    default:
      return error;
  }

  if (request_->load_flags & LOAD_BYPASS_PROXY) {
    return error;
  }

  int rv = session_->proxy_service()->ReconsiderProxyAfterError(
      request_->url, &proxy_info_, &io_callback_, &pac_request_);
  if (rv == OK || rv == ERR_IO_PENDING) {
    // If the error was during connection setup, there is no socket to
    // disconnect.
    if (connection_.socket())
      connection_.socket()->Disconnect();
    connection_.Reset();
    DCHECK(!request_headers_bytes_sent_);
    next_state_ = STATE_RESOLVE_PROXY_COMPLETE;
  } else {
    rv = error;
  }

  return rv;
}

bool HttpNetworkTransaction::ShouldApplyProxyAuth() const {
  return (proxy_mode_ == kHTTPProxy) || establishing_tunnel_;
}

bool HttpNetworkTransaction::ShouldApplyServerAuth() const {
  return !establishing_tunnel_;
}

std::string HttpNetworkTransaction::BuildAuthorizationHeader(
    HttpAuth::Target target) const {
  DCHECK(HaveAuth(target));

  // Add a Authorization/Proxy-Authorization header line.
  std::string credentials = auth_handler_[target]->GenerateCredentials(
      auth_identity_[target].username,
      auth_identity_[target].password,
      request_,
      &proxy_info_);

  return HttpAuth::GetAuthorizationHeaderName(target) +
      ": "  + credentials + "\r\n";
}

GURL HttpNetworkTransaction::AuthOrigin(HttpAuth::Target target) const {
  return target == HttpAuth::AUTH_PROXY ?
      GURL("http://" + proxy_info_.proxy_server().host_and_port()) :
      request_->url.GetOrigin();
}

std::string HttpNetworkTransaction::AuthPath(HttpAuth::Target target)
    const {
  // Proxy authentication realms apply to all paths. So we will use
  // empty string in place of an absolute path.
  return target == HttpAuth::AUTH_PROXY ?
      std::string() : request_->url.path();
}

// static
std::string HttpNetworkTransaction::AuthTargetString(
    HttpAuth::Target target) {
  return target == HttpAuth::AUTH_PROXY ? "proxy" : "server";
}

void HttpNetworkTransaction::InvalidateRejectedAuthFromCache(
    HttpAuth::Target target) {
  DCHECK(HaveAuth(target));

  // TODO(eroman): this short-circuit can be relaxed. If the realm of
  // the preemptively used auth entry matches the realm of the subsequent
  // challenge, then we can invalidate the preemptively used entry.
  // Otherwise as-is we may send the failed credentials one extra time.
  if (auth_identity_[target].source == HttpAuth::IDENT_SRC_PATH_LOOKUP)
    return;

  // Clear the cache entry for the identity we just failed on.
  // Note: we require the username/password to match before invalidating
  // since the entry in the cache may be newer than what we used last time.
  session_->auth_cache()->Remove(AuthOrigin(target),
                                 auth_handler_[target]->realm(),
                                 auth_identity_[target].username,
                                 auth_identity_[target].password);
}

bool HttpNetworkTransaction::SelectPreemptiveAuth(HttpAuth::Target target) {
  DCHECK(!HaveAuth(target));

  // Don't do preemptive authorization if the URL contains a username/password,
  // since we must first be challenged in order to use the URL's identity.
  if (request_->url.has_username())
    return false;

  // SelectPreemptiveAuth() is on the critical path for each request, so it
  // is expected to be fast. LookupByPath() is fast in the common case, since
  // the number of http auth cache entries is expected to be very small.
  // (For most users in fact, it will be 0.)

  HttpAuthCache::Entry* entry = session_->auth_cache()->LookupByPath(
      AuthOrigin(target), AuthPath(target));

  // We don't support preemptive authentication for connection-based
  // authentication schemes because they can't reuse entry->handler().
  // Hopefully we can remove this limitation in the future.
  if (entry && !entry->handler()->is_connection_based()) {
    auth_identity_[target].source = HttpAuth::IDENT_SRC_PATH_LOOKUP;
    auth_identity_[target].invalid = false;
    auth_identity_[target].username = entry->username();
    auth_identity_[target].password = entry->password();
    auth_handler_[target] = entry->handler();
    return true;
  }
  return false;
}

bool HttpNetworkTransaction::SelectNextAuthIdentityToTry(
    HttpAuth::Target target) {
  DCHECK(auth_handler_[target]);
  DCHECK(auth_identity_[target].invalid);

  // Try to use the username/password encoded into the URL first.
  // (By checking source == IDENT_SRC_NONE, we make sure that this
  // is only done once for the transaction.)
  if (target == HttpAuth::AUTH_SERVER && request_->url.has_username() &&
      auth_identity_[target].source == HttpAuth::IDENT_SRC_NONE) {
    auth_identity_[target].source = HttpAuth::IDENT_SRC_URL;
    auth_identity_[target].invalid = false;
    // TODO(wtc) It may be necessary to unescape the username and password
    // after extracting them from the URL.  We should be careful about
    // embedded nulls in that case.
    auth_identity_[target].username = ASCIIToWide(request_->url.username());
    auth_identity_[target].password = ASCIIToWide(request_->url.password());
    // TODO(eroman): If the password is blank, should we also try combining
    // with a password from the cache?
    return true;
  }

  // Check the auth cache for a realm entry.
  HttpAuthCache::Entry* entry = session_->auth_cache()->LookupByRealm(
      AuthOrigin(target), auth_handler_[target]->realm());

  if (entry) {
    // Disallow re-using of identity if the scheme of the originating challenge
    // does not match. This protects against the following situation:
    // 1. Browser prompts user to sign into DIGEST realm="Foo".
    // 2. Since the auth-scheme is not BASIC, the user is reasured that it
    //    will not be sent over the wire in clear text. So they use their
    //    most trusted password.
    // 3. Next, the browser receives a challenge for BASIC realm="Foo". This
    //    is the same realm that we have a cached identity for. However if
    //    we use that identity, it would get sent over the wire in
    //    clear text (which isn't what the user agreed to when entering it).
    if (entry->handler()->scheme() != auth_handler_[target]->scheme()) {
      LOG(WARNING) << "The scheme of realm " << auth_handler_[target]->realm()
                   << " has changed from " << entry->handler()->scheme()
                   << " to " << auth_handler_[target]->scheme();
      return false;
    }

    auth_identity_[target].source = HttpAuth::IDENT_SRC_REALM_LOOKUP;
    auth_identity_[target].invalid = false;
    auth_identity_[target].username = entry->username();
    auth_identity_[target].password = entry->password();
    return true;
  }
  return false;
}

std::string HttpNetworkTransaction::AuthChallengeLogMessage() const {
  std::string msg;
  std::string header_val;
  void* iter = NULL;
  while (response_.headers->EnumerateHeader(&iter, "proxy-authenticate",
                                            &header_val)) {
    msg.append("\n  Has header Proxy-Authenticate: ");
    msg.append(header_val);
  }

  iter = NULL;
  while (response_.headers->EnumerateHeader(&iter, "www-authenticate",
                                            &header_val)) {
    msg.append("\n  Has header WWW-Authenticate: ");
    msg.append(header_val);
  }

  // RFC 4559 requires that a proxy indicate its support of NTLM/Negotiate
  // authentication with a "Proxy-Support: Session-Based-Authentication"
  // response header.
  iter = NULL;
  while (response_.headers->EnumerateHeader(&iter, "proxy-support",
                                            &header_val)) {
    msg.append("\n  Has header Proxy-Support: ");
    msg.append(header_val);
  }

  return msg;
}

int HttpNetworkTransaction::HandleAuthChallenge() {
  DCHECK(response_.headers);

  int status = response_.headers->response_code();
  if (status != 401 && status != 407)
    return OK;
  HttpAuth::Target target = status == 407 ?
      HttpAuth::AUTH_PROXY : HttpAuth::AUTH_SERVER;

  LOG(INFO) << "The " << AuthTargetString(target) << " "
            << AuthOrigin(target) << " requested auth"
            << AuthChallengeLogMessage();

  if (target == HttpAuth::AUTH_PROXY && proxy_info_.is_direct())
    return ERR_UNEXPECTED_PROXY_AUTH;

  // The auth we tried just failed, hence it can't be valid. Remove it from
  // the cache so it won't be used again, unless it's a null identity.
  if (HaveAuth(target) &&
      auth_identity_[target].source != HttpAuth::IDENT_SRC_NONE)
    InvalidateRejectedAuthFromCache(target);

  auth_identity_[target].invalid = true;

  // Find the best authentication challenge that we support.
  HttpAuth::ChooseBestChallenge(response_.headers.get(),
                                target,
                                &auth_handler_[target]);

  if (!auth_handler_[target]) {
    if (establishing_tunnel_) {
      LOG(ERROR) << "Can't perform auth to the " << AuthTargetString(target)
                 << " " << AuthOrigin(target)
                 << " when establishing a tunnel"
                 << AuthChallengeLogMessage();

      // We are establishing a tunnel, we can't show the error page because an
      // active network attacker could control its contents.  Instead, we just
      // fail to establish the tunnel.
      DCHECK(target == HttpAuth::AUTH_PROXY);
      return ERR_PROXY_AUTH_REQUESTED;
    }
    // We found no supported challenge -- let the transaction continue
    // so we end up displaying the error page.
    return OK;
  }

  if (auth_handler_[target]->NeedsIdentity()) {
    // Pick a new auth identity to try, by looking to the URL and auth cache.
    // If an identity to try is found, it is saved to auth_identity_[target].
    SelectNextAuthIdentityToTry(target);
  } else {
    // Proceed with a null identity.
    //
    // TODO(wtc): Add a safeguard against infinite transaction restarts, if
    // the server keeps returning "NTLM".
    auth_identity_[target].source = HttpAuth::IDENT_SRC_NONE;
    auth_identity_[target].invalid = false;
    auth_identity_[target].username.clear();
    auth_identity_[target].password.clear();
  }

  // Make a note that we are waiting for auth. This variable is inspected
  // when the client calls RestartWithAuth() to pick up where we left off.
  pending_auth_target_ = target;

  if (auth_identity_[target].invalid) {
    // We have exhausted all identity possibilities, all we can do now is
    // pass the challenge information back to the client.
    PopulateAuthChallenge(target);
  }
  return OK;
}

void HttpNetworkTransaction::PopulateAuthChallenge(HttpAuth::Target target) {
  // Populates response_.auth_challenge with the authentication challenge info.
  // This info is consumed by URLRequestHttpJob::GetAuthChallengeInfo().

  AuthChallengeInfo* auth_info = new AuthChallengeInfo;
  auth_info->is_proxy = target == HttpAuth::AUTH_PROXY;
  auth_info->scheme = ASCIIToWide(auth_handler_[target]->scheme());
  // TODO(eroman): decode realm according to RFC 2047.
  auth_info->realm = ASCIIToWide(auth_handler_[target]->realm());

  std::string host_and_port;
  if (target == HttpAuth::AUTH_PROXY) {
    host_and_port = proxy_info_.proxy_server().host_and_port();
  } else {
    DCHECK(target == HttpAuth::AUTH_SERVER);
    host_and_port = GetHostAndPort(request_->url);
  }
  auth_info->host_and_port = ASCIIToWide(host_and_port);
  response_.auth_challenge = auth_info;
}

}  // namespace net
