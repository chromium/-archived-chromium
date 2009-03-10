// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/url_request_failed_dns_job.h"

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"

const char URLRequestFailedDnsJob::kTestUrl[] =
    "http://url.handled.by.fake.dns/";

void URLRequestFailedDnsJob::Start() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestFailedDnsJob::StartAsync));
}

/* static */
void URLRequestFailedDnsJob::AddUITestUrls() {
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddUrlHandler(GURL(kTestUrl),
                        &URLRequestFailedDnsJob::Factory);
}

/*static */
URLRequestJob* URLRequestFailedDnsJob::Factory(URLRequest* request,
    const std::string& scheme) {
  return new URLRequestFailedDnsJob(request);
}

void URLRequestFailedDnsJob::StartAsync() {
  NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
                                    net::ERR_NAME_NOT_RESOLVED));
}
