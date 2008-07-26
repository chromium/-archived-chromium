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

#include "net/http/http_connection.h"

#include "net/base/client_socket.h"
#include "net/http/http_connection_manager.h"

namespace net {

HttpConnection::HttpConnection(HttpConnectionManager* mgr)
    : mgr_(mgr), socket_(NULL) {
}

HttpConnection::~HttpConnection() {
  Reset();
}

int HttpConnection::Init(const std::string& group_name,
                         CompletionCallback* callback) {
  Reset();
  group_name_ = group_name;
  return mgr_->RequestSocket(group_name_, &socket_, callback);
}

void HttpConnection::Reset() {
  if (group_name_.empty())  // Was Init called?
    return;
  if (socket_) {
    mgr_->ReleaseSocket(group_name_, socket_);
    socket_ = NULL;
  } else {
    mgr_->CancelRequest(group_name_, &socket_);
  }
  group_name_.clear();
}

}  // namespace net
