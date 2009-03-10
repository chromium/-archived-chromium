// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This class simulates what wininet does when a dns lookup fails.

#ifndef CHROME_BROWSER_AUTOMATION_URL_REQUEST_FAILED_DNS_JOB_H__
#define CHROME_BROWSER_AUTOMATION_URL_REQUEST_FAILED_DNS_JOB_H__

#include "net/url_request/url_request_job.h"

class URLRequestFailedDnsJob : public URLRequestJob {
 public:
  URLRequestFailedDnsJob(URLRequest* request)
      : URLRequestJob(request) { }

  virtual void Start();

  static URLRequestJob* Factory(URLRequest* request,
                                const std::string& scheme);

  // A test URL that can be used in UI tests.
  static const char kTestUrl[];

  // For UI tests: adds the testing URLs to the URLRequestFilter.
  static void AddUITestUrls();

 private:
  // Simulate a DNS failure.
  void StartAsync();
};

#endif  // CHROME_BROWSER_AUTOMATION_URL_REQUEST_FAILED_DNS_JOB_H__
