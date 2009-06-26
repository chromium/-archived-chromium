// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/automation/url_request_failed_dns_job.h"
#include "chrome/browser/automation/url_request_mock_http_job.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

namespace {

class ResourceDispatcherTest : public UITest {
 public:
  void CheckTitleTest(const std::wstring& file,
                      const std::wstring& expected_title) {
    NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
    const int kCheckDelayMs = 100;
    int max_wait_time = 5000;
    while (max_wait_time > 0) {
      max_wait_time -= kCheckDelayMs;
      PlatformThread::Sleep(kCheckDelayMs);
      if (expected_title == GetActiveTabTitle())
        break;
    }

    EXPECT_EQ(expected_title, GetActiveTabTitle());
  }

 protected:
  ResourceDispatcherTest() : UITest() {
    dom_automation_enabled_ = true;
  }
};

TEST_F(ResourceDispatcherTest, SniffHTMLWithNoContentType) {
  CheckTitleTest(L"content-sniffer-test0.html",
                 L"Content Sniffer Test 0");
}

TEST_F(ResourceDispatcherTest, RespectNoSniffDirective) {
  CheckTitleTest(L"nosniff-test.html", L"");
}

TEST_F(ResourceDispatcherTest, DoNotSniffHTMLFromTextPlain) {
  CheckTitleTest(L"content-sniffer-test1.html", L"");
}

TEST_F(ResourceDispatcherTest, DoNotSniffHTMLFromImageGIF) {
  CheckTitleTest(L"content-sniffer-test2.html", L"");
}

TEST_F(ResourceDispatcherTest, SniffNoContentTypeNoData) {
  CheckTitleTest(L"content-sniffer-test3.html",
                 L"Content Sniffer Test 3");
  PlatformThread::Sleep(sleep_timeout_ms() * 2);
  EXPECT_EQ(1, GetTabCount());

  // Make sure the download shelf is not showing.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  bool visible = false;
  ASSERT_TRUE(browser->IsShelfVisible(&visible));
  EXPECT_FALSE(visible);
}

TEST_F(ResourceDispatcherTest, ContentDispositionEmpty) {
  CheckTitleTest(L"content-disposition-empty.html", L"success");
}

TEST_F(ResourceDispatcherTest, ContentDispositionInline) {
  CheckTitleTest(L"content-disposition-inline.html", L"success");
}

// Test for bug #1091358.
TEST_F(ResourceDispatcherTest, SyncXMLHttpRequest) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  tab->NavigateToURL(server->TestServerPageW(
      L"files/sync_xmlhttprequest.html"));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(DidSyncRequestSucceed());",
      &success));
  EXPECT_TRUE(success);
}

TEST_F(ResourceDispatcherTest, SyncXMLHttpRequest_Disallowed) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  tab->NavigateToURL(server->TestServerPageW(
      L"files/sync_xmlhttprequest_disallowed.html"));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(DidSucceed());",
      &success));
  EXPECT_TRUE(success);
}

// Test for bug #1159553 -- A synchronous xhr (whose content-type is
// downloadable) would trigger download and hang the renderer process,
// if executed while navigating to a new page.
TEST_F(ResourceDispatcherTest, SyncXMLHttpRequest_DuringUnload) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());

  tab->NavigateToURL(
      server->TestServerPageW(L"files/sync_xmlhttprequest_during_unload.html"));

  // Confirm that the page has loaded (since it changes its title during load).
  std::wstring tab_title;
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"sync xhr on unload", tab_title);

  // Navigate to a new page, to dispatch unload event and trigger xhr.
  // (the bug would make this step hang the renderer).
  bool timed_out = false;
  tab->NavigateToURLWithTimeout(server->TestServerPageW(L"files/title2.html"),
                                action_max_timeout_ms(),
                                &timed_out);
  EXPECT_FALSE(timed_out);

  // Check that the new page got loaded, and that no download was triggered.
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"Title Of Awesomeness", tab_title);

  bool shelf_is_visible = false;
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser->IsShelfVisible(&shelf_is_visible));
  EXPECT_FALSE(shelf_is_visible);
}

// Tests that onunload is run for cross-site requests.  (Bug 1114994)
TEST_F(ResourceDispatcherTest, CrossSiteOnunloadCookie) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());

  GURL url(server->TestServerPageW(L"files/onunload_cookie.html"));
  tab->NavigateToURL(url);

  // Confirm that the page has loaded (since it changes its title during load).
  std::wstring tab_title;
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"set cookie on unload", tab_title);

  // Navigate to a new cross-site page, to dispatch unload event and set the
  // cookie.
  CheckTitleTest(L"content-sniffer-test0.html",
                 L"Content Sniffer Test 0");

  // Check that the cookie was set.
  std::string value_result;
  ASSERT_TRUE(tab->GetCookieByName(url, "onunloadCookie", &value_result));
  ASSERT_FALSE(value_result.empty());
  ASSERT_STREQ("foo", value_result.c_str());
}

#if !defined(OS_MACOSX)
// Tests that the onbeforeunload and onunload logic is shortcutted if the old
// renderer is gone.  In that case, we don't want to wait for the old renderer
// to run the handlers.
// TODO(pinkerton): We need to disable this because the crash causes
// the OS CrashReporter process to kick in to analyze the poor dead renderer.
// Unfortunately, if the app isn't stripped of debug symbols, this takes about
// five minutes to complete and isn't conducive to quick turnarounds. As we
// don't currently strip the app on the build bots, this is bad times.
TEST_F(ResourceDispatcherTest, CrossSiteAfterCrash) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());

  // Cause the renderer to crash.
  // TODO(albertb): We need to disable this on Linux since
  // crash_service.exe hasn't been ported yet.
#if defined(OS_WIN)
  expected_crashes_ = 1;
#endif
  tab->NavigateToURLAsync(GURL("about:crash"));
  // Wait for browser to notice the renderer crash.
  PlatformThread::Sleep(sleep_timeout_ms());

  // Navigate to a new cross-site page.  The browser should not wait around for
  // the old renderer's on{before}unload handlers to run.
  CheckTitleTest(L"content-sniffer-test0.html",
                 L"Content Sniffer Test 0");
}
#endif  // !defined(OS_MACOSX)

// Tests that cross-site navigations work when the new page does not go through
// the BufferedEventHandler (e.g., non-http{s} URLs).  (Bug 1225872)
TEST_F(ResourceDispatcherTest, CrossSiteNavigationNonBuffered) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());

  // Start with an HTTP page.
  CheckTitleTest(L"content-sniffer-test0.html",
                 L"Content Sniffer Test 0");

  // Now load a file:// page, which does not use the BufferedEventHandler.
  // Make sure that the page loads and displays a title, and doesn't get stuck.
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title2.html");
  bool timed_out = false;
  tab->NavigateToURLWithTimeout(net::FilePathToFileURL(test_file),
                                action_max_timeout_ms(),
                                &timed_out);
  EXPECT_FALSE(timed_out);
  EXPECT_EQ(L"Title Of Awesomeness", GetActiveTabTitle());
}

// Tests that a cross-site navigation to an error page (resulting in the link
// doctor page) still runs the onunload handler and can support navigations
// away from the link doctor page.  (Bug 1235537)
TEST_F(ResourceDispatcherTest, CrossSiteNavigationErrorPage) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());

  GURL url(server->TestServerPageW(L"files/onunload_cookie.html"));
  tab->NavigateToURL(url);

  // Confirm that the page has loaded (since it changes its title during load).
  std::wstring tab_title;
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"set cookie on unload", tab_title);

  // Navigate to a new cross-site URL that results in an error page.  We must
  // wait for the error page to update the title.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491.
  tab->NavigateToURL(GURL(URLRequestFailedDnsJob::kTestUrl));
  for (int i = 0; i < 10; ++i) {
    PlatformThread::Sleep(sleep_timeout_ms());
    if (GetActiveTabTitle() != L"set cookie on unload") {
      // Success, bail out.
      break;
    }
  }
  EXPECT_NE(L"set cookie on unload", GetActiveTabTitle());

  // Check that the cookie was set, meaning that the onunload handler ran.
  std::string value_result;
  EXPECT_TRUE(tab->GetCookieByName(url, "onunloadCookie", &value_result));
  EXPECT_FALSE(value_result.empty());
  EXPECT_STREQ("foo", value_result.c_str());

  // Check that renderer-initiated navigations still work.  In a previous bug,
  // the ResourceDispatcherHost would think that such navigations were
  // cross-site, because we didn't clean up from the previous request.  Since
  // TabContents was in the NORMAL state, it would ignore the attempt to run
  // the onunload handler, and the navigation would fail.
  // (Test by redirecting to javascript:window.location='someURL'.)
  GURL test_url(server->TestServerPageW(L"files/title2.html"));
  std::string redirect_url = "javascript:window.location='" +
      test_url.possibly_invalid_spec() + "'";
  tab->NavigateToURLAsync(GURL(redirect_url));
  // Wait for JavaScript redirect to happen.
  PlatformThread::Sleep(sleep_timeout_ms() * 3);
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"Title Of Awesomeness", tab_title);
}

}  // namespace
