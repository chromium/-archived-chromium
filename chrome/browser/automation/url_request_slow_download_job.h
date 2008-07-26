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
// This class simulates a slow download.  This used in a UI test to test the
// download manager.  Requests to |kUnknownSizeUrl| and |kKnownSizeUrl| start
// downloads that pause after the first

#ifndef CHROME_BROWSER_AUTOMATION_URL_REQUEST_SLOW_DOWNLOAD_JOB_H__
#define CHROME_BROWSER_AUTOMATION_URL_REQUEST_SLOW_DOWNLOAD_JOB_H__

#include <vector>

#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_job.h"

class URLRequestSlowDownloadJob : public URLRequestJob {
 public:
  URLRequestSlowDownloadJob(URLRequest* request);
  virtual ~URLRequestSlowDownloadJob() { }

  // Timer callback, used to check to see if we should finish our download and
  // send the second chunk.
  void CheckDoneStatus();

  // URLRequestJob methods
  virtual void Start();
  virtual bool GetMimeType(std::string* mime_type);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual bool ReadRawData(char* buf, int buf_size, int *bytes_read);

  static URLRequestJob* Factory(URLRequest* request,
                                const std::string& scheme);

  // Test URLs.
  static const wchar_t kUnknownSizeUrl[];
  static const wchar_t kKnownSizeUrl[];
  static const wchar_t kFinishDownloadUrl[];

  // For UI tests: adds the testing URLs to the URLRequestFilter.
  static void AddUITestUrls();

 private:
  // Mark all pending requests to be finished.  We keep track of pending
  // requests in |kPendingRequests|.
  static void FinishPendingRequests();
  static std::vector<URLRequestSlowDownloadJob*> kPendingRequests;

  void StartAsync();

  void set_should_finish_download() { should_finish_download_ = true; }

  int first_download_size_remaining_;
  bool should_finish_download_;
  bool should_send_second_chunk_;
};

#endif  // CHROME_BROWSER_AUTOMATION_URL_REQUEST_SLOW_DOWNLOAD_JOB_H__
