// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A URLRequestJob class that pulls the content and http headers from disk.

#ifndef CHROME_BROWSER_AUTOMATION_URL_REQUEST_MOCK_HTTP_JOB_H__
#define CHROME_BROWSER_AUTOMATION_URL_REQUEST_MOCK_HTTP_JOB_H__

#include <string>

#include "net/url_request/url_request_file_job.h"

class URLRequestMockHTTPJob : public URLRequestFileJob {
 public:
  URLRequestMockHTTPJob(URLRequest* request, const FilePath& file_path);
  virtual ~URLRequestMockHTTPJob() { }

  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);

  static URLRequest::ProtocolFactory Factory;

  // For UI tests: adds the testing URLs to the URLRequestFilter.
  static void AddUITestUrls(const std::wstring& base_path);

  // Given the path to a file relative to base_path_, construct a mock URL.
  static GURL GetMockUrl(const std::wstring& path);

 private:
  void GetResponseInfoConst(net::HttpResponseInfo* info) const;

  // This is the file path leading to the root of the directory to use as the
  // root of the http server.
  static std::wstring base_path_;
};

# endif  // CHROME_BROWSER_AUTOMATION_URL_REQUEST_MOCK_HTTP_JOB_H__

