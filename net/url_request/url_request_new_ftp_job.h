// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_NEW_FTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_NEW_FTP_JOB_H_

#include <string>

#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/ftp/ftp_request_info.h"
#include "net/ftp/ftp_transaction.h"
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

  // TODO(ibrar):  Yet to give another look at this function.
  virtual uint64 GetUploadProgress() const { return 0; }
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

  void NotifyHeadersComplete();

  void DestroyTransaction();
  void StartTransaction();

  void OnStartCompleted(int result);
  void OnReadCompleted(int result);

  int ProcessFtpDir(net::IOBuffer *buf, int buf_size, int bytes_read);

  net::AuthState server_auth_state_;

  net::FtpRequestInfo request_info_;
  scoped_ptr<net::FtpTransaction> transaction_;
  const net::FtpResponseInfo* response_info_;

  scoped_refptr<net::IOBuffer> dir_listing_buf_;
  int dir_listing_buf_size_;

  net::CompletionCallbackImpl<URLRequestNewFtpJob> start_callback_;
  net::CompletionCallbackImpl<URLRequestNewFtpJob> read_callback_;

  std::string directory_html_;
  bool read_in_progress_;
  std::string encoding_;

  // Keep a reference to the url request context to be sure it's not deleted
  // before us.
  scoped_refptr<URLRequestContext> context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestNewFtpJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_NEW_FTP_JOB_H_
