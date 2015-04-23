// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpBasicStream is a simple implementation of HttpStream.  It assumes it is
// not sharing a sharing with any other HttpStreams, therefore it just reads and
// writes directly to the Http Stream.

#ifndef NET_HTTP_HTTP_BASIC_STREAM_H_
#define NET_HTTP_HTTP_BASIC_STREAM_H_

#include "base/basictypes.h"
#include "net/http/http_stream.h"
#include "net/socket/client_socket_handle.h"

namespace net {

class HttpBasicStream : public HttpStream {
 public:
  explicit HttpBasicStream(ClientSocketHandle* handle) : handle_(handle) {}
  virtual ~HttpBasicStream() {}

  // HttpStream methods:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   CompletionCallback* callback) {
    return handle_->socket()->Read(buf, buf_len, callback);
  }

  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    CompletionCallback* callback) {
    return handle_->socket()->Write(buf, buf_len, callback);
  }

 private:
  ClientSocketHandle* const handle_;

  DISALLOW_COPY_AND_ASSIGN(HttpBasicStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STREAM_H_
