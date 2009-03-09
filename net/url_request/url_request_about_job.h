// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_

#include <string>

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

class URLRequestAboutJob : public URLRequestJob {
 public:
  explicit URLRequestAboutJob(URLRequest* request);

  virtual void Start();
  virtual bool GetMimeType(std::string* mime_type) const;

  static URLRequest::ProtocolFactory Factory;

 private:
  void StartAsync();
};

#endif  // NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_

