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

// A URLRequestJob class that simulates network errors (including https
// related).
// It is based on URLRequestMockHttpJob.

#ifndef CHROME_BROWSER_AUTOMATION_URL_REQUEST_MOCK_NET_ERROR_H__
#define CHROME_BROWSER_AUTOMATION_URL_REQUEST_MOCK_NET_ERROR_H__

#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "net/base/net_errors.h"

class URLRequestMockNetErrorJob : public URLRequestMockHTTPJob {
 public:
  URLRequestMockNetErrorJob(URLRequest* request,
                            const std::vector<int>& errors,
                            X509Certificate* ssl_cert);
  virtual ~URLRequestMockNetErrorJob();

  virtual void Start();
  virtual void ContinueDespiteLastError();

  // Add the specified URL to the list of URLs that should be mocked.  When this
  // URL is hit, the specified |errors| will be played.  If any of these errors
  // is a cert error, |ssl_cert| will be used as the ssl cert when notifying of
  // the error.  |ssl_cert| can be NULL if |errors| does not contain any cert
  // errors. |base| is the location on disk where the file mocking the URL
  // contents and http-headers should be retrieved from.
  static void AddMockedURL(const GURL& url,
                           const std::wstring& base,
                           const std::vector<int>& errors,
                           X509Certificate* ssl_cert);

  // Removes the specified |url| from the list of mocked urls.
  static void RemoveMockedURL(const GURL& url);

 private:
  struct MockInfo {
    MockInfo() : ssl_cert(NULL) { }
    MockInfo(std::wstring base,
             std::vector<int> errors,
             X509Certificate* ssl_cert)
        : base(base),
          errors(errors),
          ssl_cert(ssl_cert) { }

    std::wstring base;
    std::vector<int> errors;
    scoped_refptr<X509Certificate> ssl_cert;
  };

  static URLRequest::ProtocolFactory Factory;

  void StartAsync();

  // The errors to simulate.
  std::vector<int> errors_;

  // The certificate to use for SSL errors.
  scoped_refptr<X509Certificate> ssl_cert_;

  typedef std::map<GURL, MockInfo> URLMockInfoMap;
  static URLMockInfoMap url_mock_info_map_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestMockNetErrorJob);
};

#endif  // #define CHROME_BROWSER_AUTOMATION_URL_REQUEST_MOCK_NET_ERROR_H__
