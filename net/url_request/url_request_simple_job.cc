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

#include "net/url_request/url_request_simple_job.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"

URLRequestSimpleJob::URLRequestSimpleJob(URLRequest* request)
    : URLRequestJob(request),
      data_offset_(0) {
}

void URLRequestSimpleJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestSimpleJob::StartAsync));
}

bool URLRequestSimpleJob::GetMimeType(std::string* mime_type) {
  *mime_type = mime_type_;
  return true;
}

bool URLRequestSimpleJob::GetCharset(std::string* charset) {
  *charset = charset_;
  return true;
}

bool URLRequestSimpleJob::ReadRawData(char* buf, int buf_size,
                                      int* bytes_read) {
  DCHECK(bytes_read);
  int remaining = static_cast<int>(data_.size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  memcpy(buf, data_.data() + data_offset_, buf_size);
  data_offset_ += buf_size;
  *bytes_read = buf_size;
  return true;
}

void URLRequestSimpleJob::StartAsync() {
  if (!request_)
    return;

  if (GetData(&mime_type_, &charset_, &data_)) {
    // Notify that the headers are complete
    NotifyHeadersComplete();
  } else {
    // what should the error code be?
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
                                      net::ERR_INVALID_URL));
  }
}
