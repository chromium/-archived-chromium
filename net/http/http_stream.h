// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpStream is an interface for reading and writing data to an HttpStream that
// keeps the client agnostic of the actual underlying transport layer.  This
// provides an abstraction for both a basic http stream as well as http
// pipelining implementations.
//
// NOTE(willchan): This interface is a work in progress.  It will most likely
// change, since for a pipelining implementation, the stream needs to contain
// the http parsing code.  For symmetry, the writing methods will probably
// contain the code for constructing http requests.

#ifndef NET_HTTP_HTTP_STREAM_H_
#define NET_HTTP_HTTP_STREAM_H_

#include "base/basictypes.h"
#include "net/base/completion_callback.h"

namespace net {

class IOBuffer;

class HttpStream {
 public:
  HttpStream() {}
  virtual ~HttpStream() {}

  // Reads data, up to buf_len bytes, from the socket.  The number of bytes
  // read is returned, or an error is returned upon failure.  Zero is returned
  // to indicate end-of-file.  ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result will be
  // passed to the callback when available. If the operation is not completed
  // immediately, the socket acquires a reference to the provided buffer until
  // the callback is invoked or the socket is destroyed.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   CompletionCallback* callback) = 0;

  // Writes data, up to buf_len bytes, to the socket.  Note: only part of the
  // data may be written!  The number of bytes written is returned, or an error
  // is returned upon failure.  ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result will be
  // passed to the callback when available. If the operation is not completed
  // immediately, the socket acquires a reference to the provided buffer until
  // the callback is invoked or the socket is destroyed.
  // Implementations of this method should not modify the contents of the actual
  // buffer that is written to the socket.
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    CompletionCallback* callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_H_
