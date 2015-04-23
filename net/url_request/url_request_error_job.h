// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Invalid URLs go through this URLRequestJob class rather than being passed
// to the default job handler.

#ifndef NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_

#include "net/url_request/url_request_job.h"

class URLRequestErrorJob : public URLRequestJob {
 public:
  URLRequestErrorJob(URLRequest* request, int error);

  virtual void Start();

 private:
  int error_;
  void StartAsync();
};

#endif  // NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_
