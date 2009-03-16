// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_NEW_FTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_NEW_FTP_JOB_H_

#include <string>
#include <vector>

#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/url_request/url_request_job.h"

class URLRequestContext;

// A URLRequestJob subclass that is built on top of FtpTransaction. It
// provides an implementation for FTP.
class URLRequestNewFtpJob : public URLRequestJob {
 public:

  explicit URLRequestNewFtpJob(URLRequest* request);

  virtual ~URLRequestNewFtpJob();

  static URLRequestJob* Factory(URLRequest* request, const std::string& scheme);

 private:
  // URLRequestJob methods:
  virtual void Start();
  virtual void Kill();
  virtual uint64 GetUploadProgress() const;
  virtual void GetResponseInfo();
  virtual int GetResponseCode();
  virtual bool GetMoreData();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

  void NotifyHeadersComplete();

  void DestroyTransaction();
  void StartTransaction();

  void OnStartCompleted(int result);
  void OnReadCompleted(int result);

  net::AuthState server_auth_state_;

  net::CompletionCallbackImpl<URLRequestNewFtpJob> start_callback_;
  net::CompletionCallbackImpl<URLRequestNewFtpJob> read_callback_;

  bool read_in_progress_;

  // Keep a reference to the url request context to be sure it's not deleted
  // before us.
  scoped_refptr<URLRequestContext> context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestNewFtpJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_NEW_FTP_JOB_H_
