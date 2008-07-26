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

#include <string>

#include "chrome/browser/url_fixer_upper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

using std::wstring;

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

class LoginPromptTest : public UITest {
 protected:
  LoginPromptTest()
      : UITest(),
        username_basic_(L"basicuser"),
        username_digest_(L"digestuser"),
        password_(L"secret"),
        password_bad_(L"denyme") {
  }

  TabProxy* GetActiveTabProxy() {
    scoped_ptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(window_proxy.get());

    int active_tab_index = 0;
    EXPECT_TRUE(window_proxy->GetActiveTabIndex(&active_tab_index));
    return window_proxy->GetTab(active_tab_index);
  }

  void NavigateTab(TabProxy* tab_proxy, const GURL& url) {
    ASSERT_TRUE(tab_proxy->NavigateToURL(url));
  }

  void AppendTab(const GURL& url) {
    scoped_ptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(window_proxy.get());
    EXPECT_TRUE(window_proxy->AppendTab(url));
  }

 protected:
  wstring username_basic_;
  wstring username_digest_;
  wstring password_;
  wstring password_bad_;
};

wstring ExpectedTitleFromAuth(wstring username, wstring password) {
  // The TestServer sets the title to username/password on successful login.
  return username + L"/" + password;
}

}  // namespace

// Test that "Basic" HTTP authentication works.
TEST_F(LoginPromptTest, TestBasicAuth) {
  TestServer server(kDocRoot);
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), server.TestServerPageW(L"auth-basic"));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(username_basic_, password_bad_));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  NavigateTab(tab.get(), server.TestServerPageW(L"auth-basic"));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(username_basic_, password_));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_),
            GetActiveTabTitle());
}

// Test that "Digest" HTTP authentication works.
TEST_F(LoginPromptTest, TestDigestAuth) {
  TestServer server(kDocRoot);
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), server.TestServerPageW(L"auth-digest"));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(username_digest_, password_bad_));
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  NavigateTab(tab.get(), server.TestServerPageW(L"auth-digest"));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(username_digest_, password_));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_),
            GetActiveTabTitle());
}

// Test that logging in on 2 tabs at once works.
TEST_F(LoginPromptTest, TestTwoAuths) {
  TestServer server(kDocRoot);

  ::scoped_ptr<TabProxy> basic_tab(GetActiveTabProxy());
  NavigateTab(basic_tab.get(), server.TestServerPageW(L"auth-basic"));

  AppendTab(GURL("about:blank"));
  ::scoped_ptr<TabProxy> digest_tab(GetActiveTabProxy());
  NavigateTab(digest_tab.get(), server.TestServerPageW(L"auth-digest"));

  // TODO(devint): http://b/1158262 basic_tab is not active, so this logs in to
  // a page whose tab isn't active, which isn't actually possible for the user
  // to do. I had a fix for this, but I'm reverting it to see if it makes the
  // test less flaky.
  EXPECT_TRUE(basic_tab->NeedsAuth());
  EXPECT_TRUE(basic_tab->SetAuth(username_basic_, password_));
  EXPECT_TRUE(digest_tab->NeedsAuth());
  EXPECT_TRUE(digest_tab->SetAuth(username_digest_, password_));

  wstring title;
  EXPECT_TRUE(basic_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_), title);

  EXPECT_TRUE(digest_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_), title);
}

// Test that cancelling authentication works.
TEST_F(LoginPromptTest, TestCancelAuth) {
  TestServer server(kDocRoot);
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());

  // First navigate to a test server page so we have something to go back to.
  EXPECT_TRUE(tab->NavigateToURL(server.TestServerPageW(L"a")));

  // Navigating while auth is requested is the same as cancelling.
  NavigateTab(tab.get(), server.TestServerPageW(L"auth-basic"));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->NavigateToURL(server.TestServerPageW(L"b")));
  EXPECT_FALSE(tab->NeedsAuth());

  NavigateTab(tab.get(), server.TestServerPageW(L"auth-basic"));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->GoBack());  // should bring us back to 'a'
  EXPECT_FALSE(tab->NeedsAuth());

  // Now add a page and go back, so we have something to go forward to.
  EXPECT_TRUE(tab->NavigateToURL(server.TestServerPageW(L"c")));
  EXPECT_TRUE(tab->GoBack());  // should bring us back to 'a'

  NavigateTab(tab.get(), server.TestServerPageW(L"auth-basic"));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->GoForward());  // should bring us to 'c'
  EXPECT_FALSE(tab->NeedsAuth());

  // Now test that cancelling works as expected.
  NavigateTab(tab.get(), server.TestServerPageW(L"auth-basic"));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_FALSE(tab->NeedsAuth());
  EXPECT_EQ(L"Denied: no auth", GetActiveTabTitle());
}
