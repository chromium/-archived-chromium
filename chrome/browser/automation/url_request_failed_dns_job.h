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
// This class simulates what wininet does when a dns lookup fails.

#ifndef CHROME_BROWSER_AUTOMATION_URL_REQUEST_FAILED_DNS_JOB_H__
#define CHROME_BROWSER_AUTOMATION_URL_REQUEST_FAILED_DNS_JOB_H__

#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_job.h"

class URLRequestFailedDnsJob : public URLRequestJob {
 public:
  URLRequestFailedDnsJob(URLRequest* request)
      : URLRequestJob(request) { }

  virtual void Start();

  static URLRequestJob* Factory(URLRequest* request,
                                const std::string& scheme);

  // A test URL that can be used in UI tests.
  static const wchar_t kTestUrl[];

  // For UI tests: adds the testing URLs to the URLRequestFilter.
  static void AddUITestUrls();

 private:
  // Simulate a DNS failure.
  void StartAsync();
};

#endif  // CHROME_BROWSER_AUTOMATION_URL_REQUEST_FAILED_DNS_JOB_H__
