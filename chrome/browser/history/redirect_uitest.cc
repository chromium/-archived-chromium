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

// Navigates the browser to server and client redirect pages and makes sure
// that the correct redirects are reflected in the history database. Errors
// here might indicate that WebKit changed the calls our glue layer gets in
// the case of redirects. It may also mean problems with the history system.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
// TODO(creis): Remove win_util when finished debugging ClientServerServer.
#include "base/win_util.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

class RedirectTest : public UITest {
 protected:
  RedirectTest() : UITest() {
  }
};

}  // namespace

// Tests a single server redirect
TEST_F(RedirectTest, Server) {
  TestServer server(kDocRoot);

  GURL final_url = server.TestServerPageW(std::wstring());
  GURL first_url = server.TestServerPageW(
      std::wstring(L"server-redirect?") + UTF8ToWide(final_url.spec()));

  NavigateToURL(first_url);

  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  std::vector<GURL> redirects;
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));

  ASSERT_EQ(1, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

// Tests a single client redirect.
TEST_F(RedirectTest, Client) {
  TestServer server(kDocRoot);

  GURL final_url = server.TestServerPageW(std::wstring());
  GURL first_url = server.TestServerPageW(
      std::wstring(L"client-redirect?") + UTF8ToWide(final_url.spec()));

  // We need the sleep for the client redirects, because it appears as two
  // page visits in the browser.
  NavigateToURL(first_url);
  Sleep(kWaitForActionMsec);

  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  std::vector<GURL> redirects;
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));

  ASSERT_EQ(1, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

TEST_F(RedirectTest, ClientEmptyReferer) {
    TestServer server(kDocRoot);

    GURL final_url = server.TestServerPageW(std::wstring());
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, L"file_client_redirect.html");
    GURL first_url = net::FilePathToFileURL(test_file);

    NavigateToURL(first_url);
    std::vector<GURL> redirects;
    // We need the sleeps for the client redirects, because it appears as two
    // page visits in the browser. And note for this test the browser actually
    // loads the html file on disk, rather than just getting a response from
    // the TestServer.
    for (int i = 0; i < 10; ++i) {
      Sleep(kWaitForActionMaxMsec / 10);
      scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
      ASSERT_TRUE(tab_proxy.get());
      ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
      if (!redirects.empty())
        break;
    }

    EXPECT_EQ(1, redirects.size());
    EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

// Tests to make sure a location change when a pending redirect exists isn't
// flagged as a redirect.
TEST_F(RedirectTest, ClientCancelled) {
  std::wstring first_path = test_data_directory_;
  file_util::AppendToPath(&first_path, L"cancelled_redirect_test.html");
  GURL first_url = net::FilePathToFileURL(first_path);

  NavigateToURL(first_url);
  Sleep(kWaitForActionMsec);

  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  std::vector<GURL> redirects;
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));

  // There should be no redirects from first_url, because the anchor location
  // change that occurs should not be flagged as a redirect and the meta-refresh
  // won't have fired yet.
  ASSERT_EQ(0, redirects.size());
  GURL current_url;
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&current_url));

  // Need to test final path and ref separately since constructing a file url
  // containing an anchor using FilePathToFileURL will escape the anchor as
  // %23, but in current_url the anchor will be '#'.
  std::string final_ref = "myanchor";
  std::wstring current_path;
  ASSERT_TRUE(net::FileURLToFilePath(current_url, &current_path));
  // Path should remain unchanged.
  EXPECT_EQ(StringToLowerASCII(first_path), StringToLowerASCII(current_path));
  EXPECT_EQ(final_ref, current_url.ref());
}

// Tests a client->server->server redirect
// TODO(creis): This is disabled temporarily while I figure out why it is
// failing.
TEST_F(RedirectTest, DISABLED_ClientServerServer) {
  TestServer server(kDocRoot);

  GURL final_url = server.TestServerPageW(std::wstring());
  GURL next_to_last = server.TestServerPageW(
      std::wstring(L"server-redirect?") + UTF8ToWide(final_url.spec()));
  GURL second_url = server.TestServerPageW(
      std::wstring(L"server-redirect?") + UTF8ToWide(next_to_last.spec()));
  GURL first_url = server.TestServerPageW(
      std::wstring(L"client-redirect?") + UTF8ToWide(second_url.spec()));
  std::vector<GURL> redirects;

  // We need the sleep for the client redirects, because it appears as two
  // page visits in the browser.
  NavigateToURL(first_url);

  for (int i = 0; i < 10; ++i) {
    Sleep(kWaitForActionMaxMsec / 10);
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
    if (!redirects.empty())
      break;
  }

  ASSERT_EQ(3, redirects.size());
  EXPECT_EQ(second_url.spec(), redirects[0].spec());
  EXPECT_EQ(next_to_last.spec(), redirects[1].spec());
  EXPECT_EQ(final_url.spec(), redirects[2].spec());
}

// Tests that the "#reference" gets preserved across server redirects.
TEST_F(RedirectTest, ServerReference) {
  TestServer server(kDocRoot);

  const std::string ref("reference");

  GURL final_url = server.TestServerPageW(std::wstring());
  GURL initial_url = server.TestServerPageW(
      std::wstring(L"server-redirect?") + UTF8ToWide(final_url.spec()) +
      L"#" + UTF8ToWide(ref));

  NavigateToURL(initial_url);

  GURL url = GetActiveTabURL();
  EXPECT_EQ(ref, url.ref());
}

// Test that redirect from http:// to file:// :
// A) does not crash the browser or confuse the redirect chain, see bug 1080873
// B) does not take place.
TEST_F(RedirectTest, NoHttpToFile) {
  TestServer server(kDocRoot);
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"http_to_file.html");
  GURL file_url = net::FilePathToFileURL(test_file);

  GURL initial_url = server.TestServerPageW(
    std::wstring(L"client-redirect?") + UTF8ToWide(file_url.spec()));

  NavigateToURL(initial_url);
  // UITest will check for crashes. We make sure the title doesn't match the
  // title from the file, because the nav should not have taken place.
  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  std::wstring actual_title;
  tab_proxy->GetTabTitle(&actual_title);
  EXPECT_NE(L"File!", actual_title);
}

// Ensures that non-user initiated location changes (within page) are
// flagged as client redirects. See bug 1139823.
TEST_F(RedirectTest, ClientFragments) {
  TestServer server(kDocRoot);
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"ref_redirect.html");
  GURL first_url = net::FilePathToFileURL(test_file);
  std::vector<GURL> redirects;

  NavigateToURL(first_url);
  for (int i = 0; i < 10; ++i) {
    Sleep(kWaitForActionMaxMsec / 10);
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
    if (!redirects.empty())
      break;
  }

  EXPECT_EQ(1, redirects.size());
  EXPECT_EQ(first_url.spec() + "#myanchor", redirects[0].spec());
}