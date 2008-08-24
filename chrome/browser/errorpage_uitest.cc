// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/browser/automation/url_request_failed_dns_job.h"

class ErrorPageTest : public UITest {
};

TEST_F(ErrorPageTest, DNSError) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  std::wstring test_host = UTF8ToWide(test_url.host());
  NavigateToURL(test_url);

  // Verify that the url is in the title.  Since it's set via Javascript, we
  // need to give it a chance to run.
  int i;
  std::wstring title;
  for (i = 0; i < 10; ++i) {
    Sleep(kWaitForActionMaxMsec / 10);
    title = GetActiveTabTitle();
    if (title.find(test_host) != std::wstring::npos) {
      // Success, bail out.
      break;
    }
  }

  if (i == 10) {
    FAIL() << "failed to get error page title; got " << title;
  }
};

