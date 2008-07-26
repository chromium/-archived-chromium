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
//
// This file's dependencies should be kept to a minimum so that it can be
// included in WebKit code that doesn't rely on much of common.

#ifndef BASE_URL_REQUEST_URL_REQUEST_STATUS_H__
#define BASE_URL_REQUEST_URL_REQUEST_STATUS_H__

// Respresents the result of a URL request. It encodes errors and various
// types of success.
class URLRequestStatus {
 public:
  enum Status {
    // Request succeeded, os_error() will be 0.
    SUCCESS = 0,

    // An IO request is pending, and the caller will be informed when it is
    // completed.
    IO_PENDING,

    // Request was successful but was handled by an external program, so there
    // is no response data. This usually means the current page should not be
    // navigated, but no error should be displayed. os_error will be 0.
    HANDLED_EXTERNALLY,

    // Request was cancelled programatically.
    CANCELED,

    // The request failed for some reason. os_error may have more information.
    FAILED,
  };

  URLRequestStatus() : status_(SUCCESS), os_error_(0) {}
  URLRequestStatus(Status s, int e) : status_(s), os_error_(e) {}

  Status status() const { return status_; }
  void set_status(Status s) { status_ = s; }

  int os_error() const { return os_error_; }
  void set_os_error(int e) { os_error_ = e; }

  // Returns true if the status is success, which makes some calling code more
  // convenient because this is the most common test. Note that we do NOT treat
  // HANDLED_EXTERNALLY as success. For everything except user notifications,
  // this value should be handled like an error (processing should stop).
  bool is_success() const {
    return status_ == SUCCESS || status_ == IO_PENDING;
  }

  // Returns true if the request is waiting for IO.
  bool is_io_pending() const {
    return status_ == IO_PENDING;
  }

 private:
  // Application level status
  Status status_;

  // Error code from the operating system network layer if an error was
  // encountered
  int os_error_;
};

#endif  // BASE_URL_REQUEST_URL_REQUEST_STATUS_H__
