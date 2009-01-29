// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/browser/automation/url_request_failed_dns_job.h"
#include "net/url_request/url_request_unittest.h"

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
    Sleep(sleep_timeout_ms());
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

TEST_F(ErrorPageTest, IFrame404) {
  // iframes that have 404 pages should not trigger an alternate error page.
  // In this test, the iframe sets the title of the parent page to "SUCCESS"
  // when the iframe loads.  If the iframe fails to load (because an alternate
  // error page loads instead), then the title will remain as "FAIL".
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data", NULL);
  ASSERT_TRUE(NULL != server.get());
  GURL test_url = server->TestServerPage("files/iframe404.html");
  NavigateToURL(test_url);

  // Verify that the url is in the title.  Since it's set via Javascript, we
  // need to give it a chance to run.
  int i;
  std::wstring title;
  for (i = 0; i < 10; ++i) {
    Sleep(sleep_timeout_ms());
    title = GetActiveTabTitle();
    if (title == L"SUCCESS") {
      // Success, bail out.
      break;
    }
  }

  if (i == 10) {
    FAIL() << "iframe 404 didn't load properly";
  }
};
