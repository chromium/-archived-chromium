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

#ifndef CHROME_COMMON_NET_URL_REQUEST_INTERCEPT_JOB_H__
#define CHROME_COMMON_NET_URL_REQUEST_INTERCEPT_JOB_H__

#include "base/basictypes.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "chrome/browser/chrome_plugin_host.h"
#include "chrome/common/chrome_plugin_api.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/notification_service.h"

class ChromePluginLib;

// A request job that handles network requests intercepted by a Chrome plugin.
class URLRequestInterceptJob
    : public URLRequestJob, public NotificationObserver {
 public:
  static URLRequestInterceptJob* FromCPRequest(CPRequest* request) {
    return ScopableCPRequest::GetData<URLRequestInterceptJob*>(request);
  }

  URLRequestInterceptJob(URLRequest* request, ChromePluginLib* plugin,
                         ScopableCPRequest* cprequest);
  virtual ~URLRequestInterceptJob();

  // Plugin callbacks.
  void OnStartCompleted(int result);
  void OnReadCompleted(int bytes_read);

  // URLRequestJob
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(char* buf, int buf_size, int* bytes_read);
  virtual bool GetMimeType(std::string* mime_type);
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual int GetResponseCode();
  virtual bool GetContentEncoding(std::string* encoding_type);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  void StartAsync();
  void DetachPlugin();

  scoped_ptr<ScopableCPRequest> cprequest_;
  ChromePluginLib* plugin_;
  bool got_headers_;
  char* read_buffer_;
  int read_buffer_size_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestInterceptJob);
};


#endif  // CHROME_COMMON_NET_URL_REQUEST_INTERCEPT_JOB_H__
