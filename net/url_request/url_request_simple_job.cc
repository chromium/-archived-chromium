// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_simple_job.h"

#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"

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

bool URLRequestSimpleJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return true;
}

bool URLRequestSimpleJob::GetCharset(std::string* charset) {
  *charset = charset_;
  return true;
}

bool URLRequestSimpleJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  DCHECK(bytes_read);
  int remaining = static_cast<int>(data_.size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  memcpy(buf->data(), data_.data() + data_offset_, buf_size);
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
