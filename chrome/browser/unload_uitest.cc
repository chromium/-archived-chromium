// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

class UnloadTest : public UITest {
 public:
  void CheckTitle(const std::wstring& expected_title) {
    const int kCheckDelayMs = 100;
    int max_wait_time = 5000;
    while (max_wait_time > 0) {
      max_wait_time -= kCheckDelayMs;
      Sleep(kCheckDelayMs);
      if (expected_title == GetActiveTabTitle())
        break;
    }

    EXPECT_EQ(expected_title, GetActiveTabTitle());
  }

  void NavigateToUnloadFileUsingTestServer(const std::wstring& test_filename,
                                           const std::wstring& expected_title) {
    const wchar_t kDocRoot[] = L"chrome/test/data";
    TestServer server(kDocRoot);

    std::wstring test_file = L"files/unload/";
    file_util::AppendToPath(&test_file, test_filename);

    GURL url(server.TestServerPageW(test_file));
    NavigateToURL(url);

    CheckTitle(expected_title);
  }

  void NavigateToNolistenersFileTwice() {
    NavigateToURL(
        URLRequestMockHTTPJob::GetMockUrl(L"unload/nolisteners.html"));
    CheckTitle(L"nolisteners");
    NavigateToURL(
        URLRequestMockHTTPJob::GetMockUrl(L"unload/nolisteners.html"));
    CheckTitle(L"nolisteners");
  }

  void NavigateToNolistenersFileTwiceAsync() {
    // TODO(ojan): We hit a DCHECK in RenderViewHost::OnMsgShouldCloseACK
    // if we don't sleep here.
    Sleep(400);
    NavigateToURLAsync(
        URLRequestMockHTTPJob::GetMockUrl(L"unload/nolisteners.html"));
    Sleep(400);
    NavigateToURLAsync(
        URLRequestMockHTTPJob::GetMockUrl(L"unload/nolisteners.html"));

    Sleep(2000);

    CheckTitle(L"nolisteners");
  }
};

// Navigate to a page with an infinite unload handler.
// Then two two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
TEST_F(UnloadTest, CrossSiteInfiniteUnloadAsync) {
  NavigateToUnloadFileUsingTestServer(L"unloadlooping.html", L"unloadlooping");
  NavigateToNolistenersFileTwiceAsync();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite unload handler.
// Then two two sync crosssite requests to ensure
// we correctly nav to each one. 
TEST_F(UnloadTest, CrossSiteInfiniteUnloadSync) {
  NavigateToUnloadFileUsingTestServer(L"unloadlooping.html", L"unloadlooping");
  NavigateToNolistenersFileTwice();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two async crosssite requests to ensure
// we don't get confused and think we're closing the tab.
TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadAsync) {
  NavigateToUnloadFileUsingTestServer(L"beforeunloadlooping.html", 
                                      L"beforeunloadlooping");
  NavigateToNolistenersFileTwiceAsync();
  ASSERT_TRUE(IsBrowserRunning());
}

// Navigate to a page with an infinite beforeunload handler.
// Then two two sync crosssite requests to ensure
// we correctly nav to each one. 
TEST_F(UnloadTest, CrossSiteInfiniteBeforeUnloadSync) {
  NavigateToUnloadFileUsingTestServer(L"beforeunloadlooping.html", 
                                      L"beforeunloadlooping");
  NavigateToNolistenersFileTwice();
  ASSERT_TRUE(IsBrowserRunning());
}
