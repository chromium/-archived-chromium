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
                                             X509Certificate* ssl_cert) {
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
  URLRequestMockNetErrorJob* job =
      new URLRequestMockNetErrorJob(request, mock_info.errors,
                                    mock_info.ssl_cert);

  // URLRequestMockNetErrorJob derives from URLRequestFileJob.  We set the
  // file_path_ of the job so that the URLRequestFileJob methods will do the
  // loading from the files.
  std::wstring file_url(L"file:///");
  file_url.append(mock_info.base);
  file_url.append(UTF8ToWide(url.path()));
  // Convert the file:/// URL to a path on disk.
  std::wstring file_path;
  net_util::FileURLToFilePath(GURL(file_url), &file_path);
  job->file_path_ = file_path;
  return job;
}

URLRequestMockNetErrorJob::URLRequestMockNetErrorJob(URLRequest* request,
    const std::vector<int>& errors, X509Certificate* cert)
    : URLRequestMockHTTPJob(request),
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
