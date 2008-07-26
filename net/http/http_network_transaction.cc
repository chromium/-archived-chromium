// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/http/http_network_transaction.h"

#include "base/string_util.h"
#include "net/base/client_socket_factory.h"
#include "net/base/host_resolver.h"
#include "net/base/load_flags.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_util.h"

// TODO(darin):
//  - authentication
//  - proxies (need to call ReconsiderProxyAfterError and handle SSL tunnel)
//  - ssl
//  - http/0.9
//  - header line continuations (i.e., lines that start with LWS)
//  - tolerate some junk (up to 4 bytes) in front of the HTTP/1.x status line

namespace net {

//-----------------------------------------------------------------------------

// TODO(darin): Move this onto HttpProxyInfo
static std::string GetProxyHostPort(const HttpProxyInfo& pi) {
  return WideToASCII(pi.proxy_server());
}

//-----------------------------------------------------------------------------

HttpNetworkTransaction::HttpNetworkTransaction(HttpNetworkSession* session,
                                               ClientSocketFactory* csf)
#pragma warning(suppress: 4355)
    : io_callback_(this, &HttpNetworkTransaction::OnIOComplete),
      user_callback_(NULL),
      session_(session),
      request_(NULL),
      pac_request_(NULL),
      socket_factory_(csf),
      connection_(session->connection_manager()),
      reused_socket_(false),
      using_ssl_(false),
      using_proxy_(false),
      using_tunnel_(false),
      bytes_sent_(0),
      header_buf_capacity_(0),
      header_buf_len_(0),
      header_buf_body_offset_(-1),
      content_length_(-1),  // -1 means unspecified.
      content_read_(0),
      read_buf_(NULL),
      read_buf_len_(0),
      next_state_(STATE_NONE) {
}

HttpNetworkTransaction::~HttpNetworkTransaction() {
  // If we still have an open socket, then make sure to close it so we don't
  // try to reuse it later on.
  if (connection_.is_initialized())
    connection_.set_socket(NULL);

  if (pac_request_)
    session_->proxy_service()->CancelPacRequest(pac_request_);
}

void HttpNetworkTransaction::BuildRequestHeaders() {
  std::string path;
  if (using_proxy_) {
    // TODO(darin): GURL should have a method for this.
    path = request_->url.spec();
    size_t ref_pos = path.rfind('#');
    if (ref_pos != std::string::npos)
      path.erase(ref_pos);
  } else {
    path = request_->url.PathForRequest();
  }

  request_headers_ = request_->method + " " + path + " HTTP/1.1\r\n" +
      "Host: " + request_->url.host();
  if (request_->url.IntPort() != -1)
    request_headers_ += ":" + request_->url.port();
  request_headers_ += "\r\n";

  // For compat with HTTP/1.0 servers and proxies:
  if (using_proxy_)
    request_headers_ += "Proxy-";
  request_headers_ += "Connection: keep-alive\r\n";

  if (!request_->user_agent.empty())
    request_headers_ += "User-Agent: " + request_->user_agent + "\r\n";

  // Our consumer should have made sure that this is a safe referrer.  See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (request_->referrer.is_valid())
    request_headers_ += "Referer: " + request_->referrer.spec() + "\r\n";

  // Add a content length header?
  if (request_->upload_data) {
    request_body_stream_.reset(new UploadDataStream(request_->upload_data));
    request_headers_ +=
        "Content-Length: " + Uint64ToString(request_body_stream_->size()) +
        "\r\n";
  } else if (request_->method == "POST" || request_->method == "PUT" ||
             request_->method == "HEAD") {
    // An empty POST/PUT request still needs a content length.  As for HEAD,
    // IE and Safari also add a content length header.  Presumably it is to
    // support sending a HEAD request to an URL that only expects to be sent a
    // POST or some other method that normally would have a message body.
    request_headers_ += "Content-Length: 0\r\n";
  }

  // Honor load flags that impact proxy caches.
  if (request_->load_flags & LOAD_BYPASS_CACHE) {
    request_headers_ += "Pragma: no-cache\r\nCache-Control: no-cache\r\n";
  } else if (request_->load_flags & LOAD_VALIDATE_CACHE) {
    request_headers_ += "Cache-Control: max-age=0\r\n";
  }

  // TODO(darin): Need to prune out duplicate headers.

  request_headers_ += request_->extra_headers;
  request_headers_ += "\r\n";
}

void HttpNetworkTransaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_callback_);

  // Since Run may result in Read being called, clear callback_ up front.
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
        rv = DoResolveProxy();
        break;
      case STATE_RESOLVE_PROXY_COMPLETE:
        rv = DoResolveProxyComplete(rv);
        break;
      case STATE_INIT_CONNECTION:
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_RESOLVE_HOST:
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_CONNECT:
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      case STATE_WRITE_HEADERS:
        rv = DoWriteHeaders();
        break;
      case STATE_WRITE_HEADERS_COMPLETE:
        rv = DoWriteHeadersComplete(rv);
        break;
      case STATE_WRITE_BODY:
        rv = DoWriteBody();
        break;
      case STATE_WRITE_BODY_COMPLETE:
        rv = DoWriteBodyComplete(rv);
        break;
      case STATE_READ_HEADERS:
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        break;
      case STATE_READ_BODY:
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpNetworkTransaction::DoResolveProxy() {
  DCHECK(!pac_request_);

  next_state_ = STATE_RESOLVE_PROXY_COMPLETE;

  return session_->proxy_service()->ResolveProxy(
      request_->url, &proxy_info_, &io_callback_, &pac_request_);
}

int HttpNetworkTransaction::DoResolveProxyComplete(int result) {
  next_state_ = STATE_INIT_CONNECTION;

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
  using_proxy_ = !proxy_info_.is_direct() && !using_ssl_;
  using_tunnel_ = !proxy_info_.is_direct() && using_ssl_;

  // Build the string used to uniquely identify connections of this type.
  std::string connection_group;
  if (using_proxy_ || using_tunnel_)
    connection_group = "proxy/" + GetProxyHostPort(proxy_info_) + "/";
  if (!using_proxy_)
    connection_group.append(request_->url.GetOrigin().spec());

  return connection_.Init(connection_group, &io_callback_);
}

int HttpNetworkTransaction::DoInitConnectionComplete(int result) {
  if (result < 0)
    return result;

  DCHECK(connection_.is_initialized());

  // Set the reused_socket_ flag to indicate that we are using a keep-alive
  // connection.  This flag is used to handle errors that occur while we are
  // trying to reuse a keep-alive connection.
  if (reused_socket_ = (connection_.socket() != NULL)) {
    next_state_ = STATE_WRITE_HEADERS;
  } else {
    next_state_ = STATE_RESOLVE_HOST;
  }
  return OK;
}

int HttpNetworkTransaction::DoResolveHost() {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;

  DCHECK(!resolver_.get());

  std::string host;
  int port;

  // Determine the host and port to connect to.
  if (using_proxy_ || using_tunnel_) {
    const std::string& proxy = GetProxyHostPort(proxy_info_);
    StringTokenizer t(proxy, ":");
    // TODO(darin): Handle errors here.  Perhaps HttpProxyInfo should do this
    // before claiming a proxy server configuration.
    t.GetNext();
    host = t.token();
    t.GetNext();
    port = static_cast<int>(StringToInt64(t.token()));
  } else {
    host = request_->url.host();
    port = request_->url.IntPort();
    if (port == -1) {
      if (using_ssl_) {
        port = 443;  // Default HTTPS port
      } else {
        port = 80;   // Default HTTP port
      }
    }
  }

  resolver_.reset(new HostResolver());
  return resolver_->Resolve(host, port, &addresses_, &io_callback_);
}

int HttpNetworkTransaction::DoResolveHostComplete(int result) {
  resolver_.reset();
  if (result == OK)
    next_state_ = STATE_CONNECT;
  return result;
}

int HttpNetworkTransaction::DoConnect() {
  next_state_ = STATE_CONNECT_COMPLETE;

  DCHECK(!connection_.socket());

  ClientSocket* s = socket_factory_->CreateTCPClientSocket(addresses_);

  // If we are using a direct SSL connection, then go ahead and create the SSL
  // wrapper socket now.  Otherwise, we need to first issue a CONNECT request.
  if (using_ssl_ && !using_tunnel_)
    s = socket_factory_->CreateSSLClientSocket(s, request_->url.host());

  connection_.set_socket(s);
  return connection_.socket()->Connect(&io_callback_);
}

int HttpNetworkTransaction::DoConnectComplete(int result) {
  if (result == OK)
    next_state_ = STATE_WRITE_HEADERS;
  return result;
}

int HttpNetworkTransaction::DoWriteHeaders() {
  next_state_ = STATE_WRITE_HEADERS_COMPLETE;

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_headers_.empty())
    BuildRequestHeaders();

  // Record our best estimate of the 'request time' as the time when we send
  // out the first bytes of the request headers.
  if (bytes_sent_ == 0)
    response_.request_time = Time::Now();

  const char* buf = request_headers_.data() + bytes_sent_;
  int buf_len = static_cast<int>(request_headers_.size() - bytes_sent_);
  DCHECK(buf_len > 0);

  return connection_.socket()->Write(buf, buf_len, &io_callback_);
}

int HttpNetworkTransaction::DoWriteHeadersComplete(int result) {
  if (result < 0)
    return HandleIOError(result);

  bytes_sent_ += result;
  if (bytes_sent_ < request_headers_.size()) {
    next_state_ = STATE_WRITE_HEADERS;
  } else if (request_->upload_data) {
    next_state_ = STATE_WRITE_BODY;
  } else {
    next_state_ = STATE_READ_HEADERS;
  }
  return OK;
}

int HttpNetworkTransaction::DoWriteBody() {
  next_state_ = STATE_WRITE_BODY_COMPLETE;

  DCHECK(request_->upload_data);
  DCHECK(request_body_stream_.get());

  const char* buf = request_body_stream_->buf();
  int buf_len = static_cast<int>(request_body_stream_->buf_len());

  return connection_.socket()->Write(buf, buf_len, &io_callback_);
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
    header_buf_.reset(static_cast<char*>(
        realloc(header_buf_.release(), header_buf_capacity_)));
  }

  char* buf = header_buf_.get() + header_buf_len_;
  int buf_len = header_buf_capacity_ - header_buf_len_;

  return connection_.socket()->Read(buf, buf_len, &io_callback_);
}

int HttpNetworkTransaction::DoReadHeadersComplete(int result) {
  if (result < 0)
    return HandleIOError(result);

  // Record our best estimate of the 'response time' as the time when we read
  // the first bytes of the response headers.
  if (header_buf_len_ == 0)
    response_.response_time = Time::Now();

  if (result == 0) {
    // The socket was closed before we found end-of-headers.  Assume that EOF
    // is end-of-headers.
    header_buf_body_offset_ = header_buf_len_;
  } else {
    header_buf_len_ += result;
    DCHECK(header_buf_len_ <= header_buf_capacity_);

    // TODO(darin): Check for a HTTP/0.9 response.

    int eoh = HttpUtil::LocateEndOfHeaders(header_buf_.get(), header_buf_len_);
    if (eoh != -1) {
      header_buf_body_offset_ = eoh;
    } else {
      next_state_ = STATE_READ_HEADERS;  // Read more.
      return OK;
    }
  }

  // And, we are done with the Start sequence.
  next_state_ = STATE_NONE;
  return DidReadResponseHeaders();
}

int HttpNetworkTransaction::DoReadBody() {
  DCHECK(read_buf_);
  DCHECK(read_buf_len_ > 0);
  DCHECK(connection_.is_initialized());

  next_state_ = STATE_READ_BODY_COMPLETE;

  // We may have some data remaining in the read buffer.
  if (header_buf_.get() && header_buf_body_offset_ < header_buf_len_) {
    int n = std::min(read_buf_len_, header_buf_len_ - header_buf_body_offset_);
    memcpy(read_buf_, header_buf_.get() + header_buf_body_offset_, n);
    header_buf_body_offset_ += n;
    if (header_buf_body_offset_ == header_buf_len_)
      header_buf_.reset();
    return n;
  }

  return connection_.socket()->Read(read_buf_, read_buf_len_, &io_callback_);
}

int HttpNetworkTransaction::DoReadBodyComplete(int result) {
  // We are done with the Read call.

  // Filter incoming data if appropriate.  FilterBuf may return an error.
  if (result > 0 && chunked_decoder_.get()) {
    result = chunked_decoder_->FilterBuf(read_buf_, result);
    if (result == 0) {
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
    content_read_ += result;
    if ((content_length_ != -1 && content_read_ >= content_length_) ||
        (chunked_decoder_.get() && chunked_decoder_->reached_eof())) {
      done = true;
      keep_alive = response_.headers->IsKeepAlive();
    }
  }

  // Cleanup the HttpConnection if we are done.
  if (done) {
    if (!keep_alive)
      connection_.set_socket(NULL);
    connection_.Reset();
  }

  // Clear these to avoid leaving around old state.
  read_buf_ = NULL;
  read_buf_len_ = 0;

  return result;
}

int HttpNetworkTransaction::DidReadResponseHeaders() {
  scoped_refptr<HttpResponseHeaders> headers = new HttpResponseHeaders(
      HttpUtil::AssembleRawHeaders(header_buf_.get(), header_buf_body_offset_));

  // Check for an intermediate 100 Continue response.  An origin server is
  // allowed to send this response even if we didn't ask for it, so we just
  // need to skip over it.
  if (headers->response_code() == 100) {
    header_buf_len_ = 0;
    header_buf_body_offset_ = -1;
    next_state_ = STATE_READ_HEADERS;
    return OK;
  }

  response_.headers = headers;
  response_.vary_data.Init(*request_, *response_.headers);

  // Figure how to determine EOF:

  // For certain responses, we know the content length is always 0.
  switch (response_.headers->response_code()) {
    case 204:
    case 205:
    case 304:
      content_length_ = 0;
      break;
  }

  if (content_length_ == -1) {
    // Ignore spurious chunked responses from HTTP/1.0 servers and proxies.
    // Otherwise "Transfer-Encoding: chunked" trumps "Content-Length: N"
    const std::string& status_line = response_.headers->GetStatusLine();
    if (!StartsWithASCII(status_line, "HTTP/1.0 ", true) &&
        response_.headers->HasHeaderValue("Transfer-Encoding", "chunked")) {
      chunked_decoder_.reset(new HttpChunkedDecoder());
    } else {
      content_length_ = response_.headers->GetContentLength();
      // If content_length_ is still -1, then we have to wait for the server to
      // close the connection.
    }
  }

  return OK;
}

int HttpNetworkTransaction::HandleIOError(int error) {
  switch (error) {
    // If we try to reuse a connection that the server is in the process of
    // closing, we may end up successfully writing out our request (or a
    // portion of our request) only to find a connection error when we try to
    // read from (or finish writing to) the socket.
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_ABORTED:
      if (reused_socket_ &&    // We reused a keep-alive connection.
          !header_buf_len_) {  // We have not received any response data yet.
        connection_.set_socket(NULL);
        connection_.Reset();
        bytes_sent_ = 0;
        if (request_body_stream_.get())
          request_body_stream_->Reset();
        next_state_ = STATE_INIT_CONNECTION;
        error = OK;
      }
      break;
  }
  return error;
}

void HttpNetworkTransaction::Destroy() {
  delete this;
}

int HttpNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback) {
  request_ = request_info;

  next_state_ = STATE_RESOLVE_PROXY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int HttpNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  return ERR_FAILED;  // TODO(darin): implement me!
}

int HttpNetworkTransaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  return ERR_FAILED;  // TODO(darin): implement me!
}

int HttpNetworkTransaction::Read(char* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(response_.headers);
  DCHECK(buf);
  DCHECK(buf_len > 0);

  if (!connection_.is_initialized())
    return 0;  // Treat like EOF.

  read_buf_ = buf;
  read_buf_len_ = buf_len;

  next_state_ = STATE_READ_BODY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

const HttpResponseInfo* HttpNetworkTransaction::GetResponseInfo() const {
  return response_.headers ? &response_ : NULL;
}

LoadState HttpNetworkTransaction::GetLoadState() const {
  switch (next_state_) {
    case STATE_RESOLVE_PROXY_COMPLETE:
      return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
    case STATE_RESOLVE_HOST_COMPLETE:
      return LOAD_STATE_RESOLVING_HOST;
    case STATE_CONNECT_COMPLETE:
      return LOAD_STATE_CONNECTING;
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

}  // namespace net
