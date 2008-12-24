// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
                            net::X509Certificate* ssl_cert,
                            const FilePath& file_path);
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
                           net::X509Certificate* ssl_cert);

  // Removes the specified |url| from the list of mocked urls.
  static void RemoveMockedURL(const GURL& url);

 private:
  struct MockInfo {
    MockInfo() : ssl_cert(NULL) { }
    MockInfo(std::wstring base,
             std::vector<int> errors,
             net::X509Certificate* ssl_cert)
        : base(base),
          errors(errors),
          ssl_cert(ssl_cert) { }

    std::wstring base;
    std::vector<int> errors;
    scoped_refptr<net::X509Certificate> ssl_cert;
  };

  static URLRequest::ProtocolFactory Factory;

  void StartAsync();

  // The errors to simulate.
  std::vector<int> errors_;

  // The certificate to use for SSL errors.
  scoped_refptr<net::X509Certificate> ssl_cert_;

  typedef std::map<GURL, MockInfo> URLMockInfoMap;
  static URLMockInfoMap url_mock_info_map_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestMockNetErrorJob);
};

#endif  // #define CHROME_BROWSER_AUTOMATION_URL_REQUEST_MOCK_NET_ERROR_H__

