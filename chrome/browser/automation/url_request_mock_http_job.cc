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

#include <windows.h>

#include "chrome/browser/automation/url_request_mock_http_job.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_filter.h"

static const char kMockHostname[] = "mock.http";
static const wchar_t kMockHeaderFileSuffix[] = L".mock-http-headers";

std::wstring URLRequestMockHTTPJob::base_path_ = L"";

/* static */
URLRequestJob* URLRequestMockHTTPJob::Factory(URLRequest* request,
                                              const std::string& scheme) {
  std::wstring file_url(L"file:///");
  file_url += base_path_;
  const std::string& url = request->url().spec();
  // Fix up the url to be the file url we're loading from disk.
  std::string host_prefix("http://");
  host_prefix.append(kMockHostname);
  size_t host_prefix_len = host_prefix.length();
  if (url.compare(0, host_prefix_len, host_prefix.data(),
                  host_prefix_len) == 0) {
    file_url += UTF8ToWide(url.substr(host_prefix_len));
  }

  // Convert the file:/// URL to a path on disk.
  std::wstring file_path;
  net::FileURLToFilePath(GURL(file_url), &file_path);
  URLRequestMockHTTPJob* job = new URLRequestMockHTTPJob(request);
  job->file_path_ = file_path;
  return job;
}

/* static */
void URLRequestMockHTTPJob::AddUITestUrls(const std::wstring& base_path) {
  base_path_ = base_path;

  // Add kMockHostname to URLRequestFilter.
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddHostnameHandler("http", kMockHostname,
                             URLRequestMockHTTPJob::Factory);
}

/* static */
GURL URLRequestMockHTTPJob::GetMockUrl(const std::wstring& path) {
  std::wstring url = L"http://";
  url.append(UTF8ToWide(kMockHostname));
  url.append(L"/");
  url.append(path);
  return GURL(url);
}

URLRequestMockHTTPJob::URLRequestMockHTTPJob(URLRequest* request)
    : URLRequestFileJob(request) { }

void URLRequestMockHTTPJob::GetResponseInfo(net::HttpResponseInfo* info) {
  std::wstring header_file = file_path_ + kMockHeaderFileSuffix;
  std::string raw_headers;
  if (!file_util::ReadFileToString(header_file, &raw_headers))
    return;

  // ParseRawHeaders expects \0 to end each header line.
  ReplaceSubstringsAfterOffset(&raw_headers, 0, "\n", std::string("\0", 1));
  info->headers = new net::HttpResponseHeaders(raw_headers);
}

bool URLRequestMockHTTPJob::GetMimeType(std::string* mime_type) {
  net::HttpResponseInfo info;
  GetResponseInfo(&info);
  return info.headers && info.headers->GetMimeType(mime_type);
}

bool URLRequestMockHTTPJob::GetCharset(std::string* charset) {
  net::HttpResponseInfo info;
  GetResponseInfo(&info);
  return info.headers && info.headers->GetCharset(charset);
}
