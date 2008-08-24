// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
#define NET_HTTP_HTTP_NETWORK_TRANSACTION_H_

#include <string>

#include "base/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/client_socket_handle.h"
#include "net/base/host_resolver.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"

namespace net {

class ClientSocketFactory;
class HostResolver;
class HttpChunkedDecoder;
class HttpNetworkSession;
class UploadDataStream;

class HttpNetworkTransaction : public HttpTransaction {
 public:
  HttpNetworkTransaction(HttpNetworkSession* session,
                         ClientSocketFactory* socket_factory);

  // HttpTransaction methods:
  virtual void Destroy();
  virtual int Start(const HttpRequestInfo* request_info,
                    CompletionCallback* callback);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 private:
  ~HttpNetworkTransaction();
  void BuildRequestHeaders();
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
  int DoWriteHeaders();
  int DoWriteHeadersComplete(int result);
  int DoWriteBody();
  int DoWriteBodyComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);

  // Called when read_buf_ contains the complete response headers.
  int DidReadResponseHeaders();

  // Called to possibly recover from the given error.  Sets next_state_ and
  // returns OK if recovering from the error.  Otherwise, the same error code
  // is returned.
  int HandleIOError(int error);

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
  enum { kHeaderBufInitialSize = 4096 };

  // Indicates the content length remaining to read.  If this value is less
  // than zero (and chunked_decoder_ is null), then we read until the server
  // closes the connection.
  int64 content_length_;

  // Keeps track of the number of response body bytes read so far.
  int64 content_read_;

  scoped_ptr<HttpChunkedDecoder> chunked_decoder_;

  // User buffer and length passed to the Read method.
  char* read_buf_;
  int read_buf_len_;

  enum State {
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,
    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
    STATE_WRITE_HEADERS,
    STATE_WRITE_HEADERS_COMPLETE,
    STATE_WRITE_BODY,
    STATE_WRITE_BODY_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_NONE
  };
  State next_state_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_TRANSACTION_H_

