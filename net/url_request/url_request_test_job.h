// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_

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
// When a job is created, it gets put on a queue of pending test jobs. To
// process jobs on this queue, use ProcessOnePendingMessage, which will process
// one step of the next job. If the job is incomplete, it will be added to the
// end of the queue.
class URLRequestTestJob : public URLRequestJob {
 public:
  URLRequestTestJob(URLRequest* request);
  virtual ~URLRequestTestJob() {}

  // the three URLs this handler will respond to
  // FIXME(brettw): we should probably also have a redirect one
  static GURL test_url_1();
  static GURL test_url_2();
  static GURL test_url_3();
  static GURL test_url_error();

  // the data that corresponds to each of the URLs above
  static std::string test_data_1();
  static std::string test_data_2();
  static std::string test_data_3();

  // Processes one pending message from the stack, returning true if any
  // message was processed, or false if there are no more pending request
  // notifications to send.
  static bool ProcessOnePendingMessage();

  // Factory method for protocol factory registration if callers don't subclass
  static URLRequest::ProtocolFactory Factory;

  // Job functions
  virtual void Start();
  virtual bool ReadRawData(char* buf, int buf_size, int *bytes_read);
  virtual void Kill();
  virtual bool GetMimeType(std::string* mime_type);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);

 protected:
  // This is what operation we are going to do next when this job is handled.
  // When the stage is DONE, this job will not be put on the queue.
  enum Stage { WAITING, DATA_AVAILABLE, ALL_DATA, DONE };

  // Call to process the next opeation, usually sending a notification, and
  // advancing the stage if necessary. THIS MAY DELETE THE OBJECT, we will
  // return false if the operations are complete, true if there are more.
  bool ProcessNextOperation();

  // Called via InvokeLater to cause callbacks to occur after Start() returns.
  void StartAsync();

  Stage stage_;

  // The data to send, will be set in Start()
  std::string data_;

  // current offset within data_
  int offset_;

  // Holds the buffer for an asynchronous ReadRawData call
  char* async_buf_;
  int async_buf_size_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_

