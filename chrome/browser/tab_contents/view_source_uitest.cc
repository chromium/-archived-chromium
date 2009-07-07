// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

class ViewSourceTest : public UITest {
 protected:
  ViewSourceTest() : UITest() {
    test_html_ = L"files/viewsource/test.html";
  }

  bool IsPageMenuCommandEnabled(int command) {
    scoped_refptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    if (!window_proxy.get())
      return false;

    bool timed_out;
    return window_proxy->IsPageMenuCommandEnabledWithTimeout(
        command, 5000, &timed_out) && !timed_out;
  }

 protected:
   std::wstring test_html_;
};

// This test renders a page in view-source and then checks to see if a cookie
// set in the html was set successfully (it shouldn't because we rendered the
// page in view source)
TEST_F(ViewSourceTest, DoesBrowserRenderInViewSource) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  std::string cookie = "viewsource_cookie";
  std::string cookie_data = "foo";

  // First we navigate to our view-source test page
  GURL url = server->TestServerPageW(test_html_);
  url = GURL("view-source:" + url.spec());
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  tab->NavigateToURL(url);
  PlatformThread::Sleep(sleep_timeout_ms());

  // Try to retrieve the cookie that the page sets
  // It should not be there (because we are in view-source mode
  std::string cookie_found;
  tab->GetCookieByName(url, cookie, &cookie_found);
  EXPECT_NE(cookie_data, cookie_found);
}

// This test renders a page normally and then renders the same page in
// view-source mode. This is done since we had a problem at one point during
// implementation of the view-source: prefix being consumed (removed from the
// URL) if the URL was not changed (apart from adding the view-source prefix)
TEST_F(ViewSourceTest, DoesBrowserConsumeViewSourcePrefix) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to google.html
  GURL url = server->TestServerPageW(test_html_);
  NavigateToURL(url);

  // Then we navigate to the SAME url but with the view-source: prefix
  GURL url_viewsource = GURL("view-source:" + url.spec());
  NavigateToURL(url_viewsource);

  // The URL should still be prefixed with view-source:
  EXPECT_EQ(url_viewsource.spec(), GetActiveTabURL().spec());
}

// Make sure that when looking at the actual page, we can select
// "View Source" from the Page menu.
TEST_F(ViewSourceTest, ViewSourceInPageMenuEnabledOnANormalPage) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to google.html
  GURL url = server->TestServerPageW(test_html_);
  NavigateToURL(url);

  EXPECT_TRUE(IsPageMenuCommandEnabled(IDC_VIEW_SOURCE));
}

// Make sure that when looking at the page source, we can't select
// "View Source" from the Page menu.
TEST_F(ViewSourceTest, ViewSourceInPageMenuDisabledWhileViewingSource) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to google.html
  GURL url = server->TestServerPageW(test_html_);
  GURL url_viewsource = GURL("view-source:" + url.spec());
  NavigateToURL(url_viewsource);

  EXPECT_FALSE(IsPageMenuCommandEnabled(IDC_VIEW_SOURCE));
}

}  // namespace
