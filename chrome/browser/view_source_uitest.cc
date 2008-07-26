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
    scoped_ptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    if (!window_proxy.get())
      return false;

    bool timed_out;
    return window_proxy->IsPageMenuCommandEnabledWithTimeout(
        command, 5000, &timed_out) && !timed_out;
  }

 protected:
   std::wstring test_html_;
};

}  // namespace

// This test renders a page in view-source and then checks to see if a cookie
// set in the html was set successfully (it shouldn't because we rendered the
// page in view source)
TEST_F(ViewSourceTest, DoesBrowserRenderInViewSource) {
  TestServer server(kDocRoot);
  std::string cookie = "viewsource_cookie";
  std::string cookie_data = "foo";

  // First we navigate to our view-source test page
  GURL url = server.TestServerPageW(test_html_);
  url = GURL("view-source:" + url.spec());
  scoped_ptr<TabProxy> tab(GetActiveTab());
  tab->NavigateToURL(url);
  Sleep(kWaitForActionMsec);

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
  TestServer server(kDocRoot);

  // First we navigate to google.html
  GURL url = server.TestServerPageW(test_html_);
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
  TestServer server(kDocRoot);

  // First we navigate to google.html
  GURL url = server.TestServerPageW(test_html_);
  NavigateToURL(url);

  EXPECT_TRUE(IsPageMenuCommandEnabled(IDC_VIEWSOURCE));
}

// Make sure that when looking at the page source, we can't select
// "View Source" from the Page menu.
TEST_F(ViewSourceTest, ViewSourceInPageMenuDisabledWhileViewingSource) {
  TestServer server(kDocRoot);

  // First we navigate to google.html
  GURL url = server.TestServerPageW(test_html_);
  GURL url_viewsource = GURL("view-source:" + url.spec());
  NavigateToURL(url_viewsource);

  EXPECT_FALSE(IsPageMenuCommandEnabled(IDC_VIEWSOURCE));
}
