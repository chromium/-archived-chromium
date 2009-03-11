// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple implementation of about: protocol handler that treats everything as
// about:blank.  No other about: features should be available to web content,
// so they're not implemented here.

#include "net/url_request/url_request_about_job.h"

#include "base/message_loop.h"

// static
URLRequestJob* URLRequestAboutJob::Factory(URLRequest* request,
                                           const std::string& scheme) {
  return new URLRequestAboutJob(request);
}

URLRequestAboutJob::URLRequestAboutJob(URLRequest* request)
    : URLRequestJob(request) {
}

void URLRequestAboutJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestAboutJob::StartAsync));
}

bool URLRequestAboutJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

void URLRequestAboutJob::StartAsync() {
  NotifyHeadersComplete();
}
