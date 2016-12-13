// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This class simulates what wininet does when a dns lookup fails.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_URL_REQUEST_JOB_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_URL_REQUEST_JOB_H_

#include "chrome/common/ref_counted_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_job.h"

class AutomationResourceMessageFilter;

namespace IPC {
class Message;
struct AutomationURLResponse;
};

// URLRequestJob implementation that loads the resources using
// automation.
class URLRequestAutomationJob : public URLRequestJob {
 public:
  URLRequestAutomationJob(
      URLRequest* request, int tab, AutomationResourceMessageFilter* filter);
  virtual ~URLRequestAutomationJob();

  // Register an interceptor for URL requests.
  static bool InitializeInterceptor();

  // URLRequestJob methods.
  virtual void Start();
  virtual void Kill();
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual int GetResponseCode() const;
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  // Peek and process automation messages for URL requests.
  static int MayFilterMessage(const IPC::Message& message);
  void OnMessage(const IPC::Message& message);

  int id() const {
    return id_;
  }

 protected:
  // Protected URLRequestJob override.
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);

  void StartAsync();
  void Cleanup();
  void DisconnectFromMessageFilter();

  // IPC message handlers.
  void OnRequestStarted(int tab, int id,
      const IPC::AutomationURLResponse& response);
  void OnDataAvailable(int tab, int id, const std::string& bytes);
  void OnRequestEnd(int tab, int id, const URLRequestStatus& status);

 private:
  int id_;
  int tab_;
  scoped_refptr<AutomationResourceMessageFilter> message_filter_;

  scoped_refptr<net::IOBuffer> pending_buf_;
  size_t pending_buf_size_;

  std::string mime_type_;
  scoped_refptr<net::HttpResponseHeaders> headers_;

  static int instance_count_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestAutomationJob);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_URL_REQUEST_JOB_H_

