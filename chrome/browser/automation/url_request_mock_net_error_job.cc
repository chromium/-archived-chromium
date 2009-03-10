// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/url_request_mock_net_error_job.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_filter.h"

// static
URLRequestMockNetErrorJob::URLMockInfoMap
    URLRequestMockNetErrorJob::url_mock_info_map_;

// static
void URLRequestMockNetErrorJob::AddMockedURL(const GURL& url,
                                             const std::wstring& base,
                                             const std::vector<int>& errors,
                                             net::X509Certificate* ssl_cert) {
#ifndef NDEBUG
  URLMockInfoMap::const_iterator iter = url_mock_info_map_.find(url);
  DCHECK(iter == url_mock_info_map_.end());
#endif

  url_mock_info_map_[url] = MockInfo(base, errors, ssl_cert);
  URLRequestFilter::GetInstance()
      ->AddUrlHandler(url, &URLRequestMockNetErrorJob::Factory);
}

// static
void URLRequestMockNetErrorJob::RemoveMockedURL(const GURL& url) {
  URLMockInfoMap::iterator iter = url_mock_info_map_.find(url);
  DCHECK(iter != url_mock_info_map_.end());
  url_mock_info_map_.erase(iter);
  URLRequestFilter::GetInstance()->RemoveUrlHandler(url);
}

// static
URLRequestJob* URLRequestMockNetErrorJob::Factory(URLRequest* request,
                                                  const std::string& scheme) {
  GURL url = request->url();

  URLMockInfoMap::const_iterator iter = url_mock_info_map_.find(url);
  DCHECK(iter != url_mock_info_map_.end());

  MockInfo mock_info = iter->second;

  // URLRequestMockNetErrorJob derives from URLRequestFileJob.  We pass a
  // FilePath so that the URLRequestFileJob methods will do the loading from
  // the files.
  std::wstring file_url(L"file:///");
  file_url.append(mock_info.base);
  file_url.append(UTF8ToWide(url.path()));
  // Convert the file:/// URL to a path on disk.
  std::wstring file_path;
  net::FileURLToFilePath(GURL(WideToUTF8(file_url)), &file_path);
  return new URLRequestMockNetErrorJob(request, mock_info.errors,
                                       mock_info.ssl_cert,
                                       FilePath::FromWStringHack(file_path));
}

URLRequestMockNetErrorJob::URLRequestMockNetErrorJob(URLRequest* request,
    const std::vector<int>& errors, net::X509Certificate* cert,
    const FilePath& file_path)
    : URLRequestMockHTTPJob(request, file_path),
      errors_(errors),
      ssl_cert_(cert) {
}

URLRequestMockNetErrorJob::~URLRequestMockNetErrorJob() {
}

void URLRequestMockNetErrorJob::Start() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestMockNetErrorJob::StartAsync));
}

void URLRequestMockNetErrorJob::StartAsync() {
  if (errors_.empty()) {
    URLRequestMockHTTPJob::Start();
  } else {
    int error =  errors_[0];
    errors_.erase(errors_.begin());

    if (net::IsCertificateError(error)) {
      DCHECK(ssl_cert_);
      request_->delegate()->OnSSLCertificateError(request_, error,
                                                  ssl_cert_.get());
    } else {
      NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, error));
    }
  }
}

void URLRequestMockNetErrorJob::ContinueDespiteLastError() {
  Start();
}
