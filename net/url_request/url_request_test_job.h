// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_

#include <string>

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

// This job type is designed to help with simple unit tests. To use, you
// probably want to inherit from it to set up the state you want. Then install
// it as the protocol handler for the "test" scheme.
//
// It will respond to three URLs, which you can retrieve using the test_url*
// getters, which will in turn respond with the corresponding responses returned
// by test_data*. Any other URLs that begin with "test:" will return an error,
// which might also be useful, you can use test_url_error() to retreive a
// standard one.
//
// You can override the known URLs or the response data by overriding Start().
//
// Optionally, you can also construct test jobs to return a headers and data
// provided to the contstructor in response to any request url.
//
// When a job is created, it gets put on a queue of pending test jobs. To
// process jobs on this queue, use ProcessOnePendingMessage, which will process
// one step of the next job. If the job is incomplete, it will be added to the
// end of the queue.
//
// Optionally, you can also construct test jobs that advance automatically
// without having to call ProcessOnePendingMessage.
class URLRequestTestJob : public URLRequestJob {
 public:
  // Constructs a job to return one of the canned responses depending on the
  // request url, with auto advance disabled.
  explicit URLRequestTestJob(URLRequest* request);

  // Constructs a job to return one of the canned responses depending on the
  // request url, optionally with auto advance enabled.
  explicit URLRequestTestJob(URLRequest* request, bool auto_advance);

  // Constructs a job to return the given response regardless of the request
  // url. The headers should include the HTTP status line and be formatted as
  // expected by net::HttpResponseHeaders.
  explicit URLRequestTestJob(URLRequest* request,
                             const std::string& response_headers,
                             const std::string& response_data,
                             bool auto_advance);

  virtual ~URLRequestTestJob();

  // The three canned URLs this handler will respond to without having been
  // explicitly initialized with response headers and data.
  // FIXME(brettw): we should probably also have a redirect one
  static GURL test_url_1();
  static GURL test_url_2();
  static GURL test_url_3();
  static GURL test_url_error();

  // The data that corresponds to each of the URLs above
  static std::string test_data_1();
  static std::string test_data_2();
  static std::string test_data_3();

  // The headers that correspond to each of the URLs above
  static std::string test_headers();

  // The headers for a redirect response
  static std::string test_redirect_headers();

  // The headers for a server error response
  static std::string test_error_headers();

  // Processes one pending message from the stack, returning true if any
  // message was processed, or false if there are no more pending request
  // notifications to send. This is not applicable when using auto_advance.
  static bool ProcessOnePendingMessage();

  // With auto advance enabled, the job will advance thru the stages without
  // the caller having to call ProcessOnePendingMessage. Auto advance depends
  // on having a message loop running. The default is to not auto advance.
  // Should not be altered after the job has started.
  bool auto_advance() { return auto_advance_; }
  void set_auto_advance(bool auto_advance) { auto_advance_ = auto_advance; }

  // Factory method for protocol factory registration if callers don't subclass
  static URLRequest::ProtocolFactory Factory;

  // Job functions
  virtual void Start();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);
  virtual void Kill();
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

 protected:
  // This is what operation we are going to do next when this job is handled.
  // When the stage is DONE, this job will not be put on the queue.
  enum Stage { WAITING, DATA_AVAILABLE, ALL_DATA, DONE };

  // Call to process the next opeation, usually sending a notification, and
  // advancing the stage if necessary. THIS MAY DELETE THE OBJECT.
  void ProcessNextOperation();

  // Call to move the job along to the next operation.
  void AdvanceJob();

  // Called via InvokeLater to cause callbacks to occur after Start() returns.
  virtual void StartAsync();

  bool auto_advance_;

  Stage stage_;

  // The headers the job should return, will be set in Start() if not provided
  // in the explicit ctor.
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  // The data to send, will be set in Start() if not provided in the explicit
  // ctor.
  std::string response_data_;

  // current offset within response_data_
  int offset_;

  // Holds the buffer for an asynchronous ReadRawData call
  net::IOBuffer* async_buf_;
  int async_buf_size_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_
