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

#ifndef NET_BASE_SOCKET_H_
#define NET_BASE_SOCKET_H_

#include "net/base/completion_callback.h"

namespace net {

// Represents a read/write socket.
class Socket {
 public:
  virtual ~Socket() {}

  // Read data, up to buf_len bytes, from the socket.  The number of bytes read
  // is returned, or an error is returned upon failure.  Zero is returned to
  // indicate end-of-file.  ERR_IO_PENDING is returned if the operation could
  // not be completed synchronously, in which case the result will be passed to
  // the callback when available.
  virtual int Read(char* buf, int buf_len,
                   CompletionCallback* callback) = 0;

  // Writes data, up to buf_len bytes, to the socket.  Note: only part of the
  // data may be written!  The number of bytes written is returned, or an error
  // is returned upon failure.  ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result will be
  // passed to the callback when available.
  virtual int Write(const char* buf, int buf_len,
                    CompletionCallback* callback) = 0;
};

}  // namespace net

#endif  // NET_BASE_SOCKET_H_
