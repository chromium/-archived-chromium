// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "net/url_request/url_request_test_job.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

// This emulates the global message loop for the test URL request class, since
// this is only test code, it's probably not too dangerous to have this static
// object.
static std::vector< scoped_refptr<URLRequestTestJob> > pending_jobs;

// static getters for known URLs
GURL URLRequestTestJob::test_url_1() {
  return GURL("test:url1");
}
GURL URLRequestTestJob::test_url_2() {
  return GURL("test:url2");
}
GURL URLRequestTestJob::test_url_3() {
  return GURL("test:url3");
}
GURL URLRequestTestJob::test_url_error() {
  return GURL("test:error");
}

// static getters for known URL responses
std::string URLRequestTestJob::test_data_1() {
  return std::string("<html><title>Test One</title></html>");
}
std::string URLRequestTestJob::test_data_2() {
  return std::string("<html><title>Test Two Two</title></html>");
}
std::string URLRequestTestJob::test_data_3() {
  return std::string("<html><title>Test Three Three Three</title></html>");
}

// static
URLRequestJob* URLRequestTestJob::Factory(URLRequest* request,
                                          const std::string& scheme) {
  return new URLRequestTestJob(request);
}

URLRequestTestJob::URLRequestTestJob(URLRequest* request)
    : URLRequestJob(request),
      stage_(WAITING),
      offset_(0),
      async_buf_(NULL),
      async_buf_size_(0) {
}

// Force the response to set a reasonable MIME type
bool URLRequestTestJob::GetMimeType(std::string* mime_type) {
  DCHECK(mime_type);
  *mime_type = "text/html";
  return true;
}

void URLRequestTestJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
       this, &URLRequestTestJob::StartAsync));
}

void URLRequestTestJob::StartAsync() {
  if (request_->url().spec() == test_url_1().spec()) {
    data_ = test_data_1();
    stage_ = DATA_AVAILABLE;  // Simulate a synchronous response for this one.
  } else if (request_->url().spec() == test_url_2().spec()) {
    data_ = test_data_2();
  } else if (request_->url().spec() == test_url_3().spec()) {
    data_ = test_data_3();
  } else {
    // unexpected url, return error
    // FIXME(brettw) we may want to use WININET errors or have some more types
    // of errors
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                net::ERR_INVALID_URL));
    // FIXME(brettw): this should emulate a network error, and not just fail
    // initiating a connection
    return;
  }

  pending_jobs.push_back(scoped_refptr<URLRequestTestJob>(this));

  this->NotifyHeadersComplete();
}

bool URLRequestTestJob::ReadRawData(char* buf, int buf_size, int *bytes_read) {
  if (stage_ == WAITING) {
    async_buf_ = buf;
    async_buf_size_ = buf_size;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
    return false;
  }

  DCHECK(bytes_read);
  *bytes_read = 0;

  if (offset_ >= static_cast<int>(data_.length())) {
    return true;  // done reading
  }

  int to_read = buf_size;
  if (to_read + offset_ > static_cast<int>(data_.length()))
    to_read = static_cast<int>(data_.length()) - offset_;

  memcpy(buf, &data_.c_str()[offset_], to_read);
  offset_ += to_read;

  *bytes_read = to_read;
  return true;
}

void URLRequestTestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  const std::string kResponseHeaders = StringPrintf(
      "HTTP/1.1 200 OK%c"
      "Content-type: text/html%c"
      "%c", 0, 0, 0);
  info->headers = new net::HttpResponseHeaders(kResponseHeaders);
}

void URLRequestTestJob::Kill() {
  if (request_) {
    // Note that this state will still cause a NotifyDone to get called
    // in ProcessNextOperation, which is required for jobs.
    stage_ = ALL_DATA;
    pending_jobs.push_back(scoped_refptr<URLRequestTestJob>(this));
  }
}

bool URLRequestTestJob::ProcessNextOperation() {
  switch (stage_) {
    case WAITING:
      stage_ = DATA_AVAILABLE;
      // OK if ReadRawData wasn't called yet.
      if (async_buf_) {
        int bytes_read;
        if (!ReadRawData(async_buf_, async_buf_size_, &bytes_read))
          NOTREACHED() << "This should not return false in DATA_AVAILABLE.";
        SetStatus(URLRequestStatus());  // clear the io pending flag
        NotifyReadComplete(bytes_read);
      }
      break;
    case DATA_AVAILABLE:
      stage_ = ALL_DATA;  // done sending data
      break;
    case ALL_DATA:
      stage_ = DONE;
      return false;
    case DONE:
      return false;
    default:
      NOTREACHED() << "Invalid stage";
      return false;
  }
  return true;
}

// static
bool URLRequestTestJob::ProcessOnePendingMessage() {
  if (pending_jobs.empty())
    return false;

  scoped_refptr<URLRequestTestJob> next_job(pending_jobs[0]);
  pending_jobs.erase(pending_jobs.begin());

  if (next_job->ProcessNextOperation())
    pending_jobs.push_back(next_job);

  return true;
}

