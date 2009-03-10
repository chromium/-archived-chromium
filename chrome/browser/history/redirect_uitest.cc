// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  scoped_refptr<HTTPTestServer> server =
    HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  GURL final_url = server->TestServerPageW(std::wstring());
  GURL first_url = server->TestServerPageW(
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
  scoped_refptr<HTTPTestServer> server =
    HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  GURL final_url = server->TestServerPageW(std::wstring());
  GURL first_url = server->TestServerPageW(
      std::wstring(L"client-redirect?") + UTF8ToWide(final_url.spec()));

  // We need the sleep for the client redirects, because it appears as two
  // page visits in the browser.
  NavigateToURL(first_url);
  Sleep(action_timeout_ms());

  scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  std::vector<GURL> redirects;
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));

  ASSERT_EQ(1, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

TEST_F(RedirectTest, ClientEmptyReferer) {
  scoped_refptr<HTTPTestServer> server =
    HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  GURL final_url = server->TestServerPageW(std::wstring());
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
    Sleep(sleep_timeout_ms());
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
  Sleep(action_timeout_ms());

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
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  GURL final_url = server->TestServerPageW(std::wstring());
  GURL next_to_last = server->TestServerPageW(
      std::wstring(L"server-redirect?") + UTF8ToWide(final_url.spec()));
  GURL second_url = server->TestServerPageW(
      std::wstring(L"server-redirect?") + UTF8ToWide(next_to_last.spec()));
  GURL first_url = server->TestServerPageW(
      std::wstring(L"client-redirect?") + UTF8ToWide(second_url.spec()));
  std::vector<GURL> redirects;

  // We need the sleep for the client redirects, because it appears as two
  // page visits in the browser.
  NavigateToURL(first_url);

  for (int i = 0; i < 10; ++i) {
    Sleep(sleep_timeout_ms());
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
  scoped_refptr<HTTPTestServer> server =
    HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  const std::string ref("reference");

  GURL final_url = server->TestServerPageW(std::wstring());
  GURL initial_url = server->TestServerPageW(
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
  scoped_refptr<HTTPTestServer> server =
    HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"http_to_file.html");
  GURL file_url = net::FilePathToFileURL(test_file);

  GURL initial_url = server->TestServerPageW(
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
  scoped_refptr<HTTPTestServer> server =
    HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"ref_redirect.html");
  GURL first_url = net::FilePathToFileURL(test_file);
  std::vector<GURL> redirects;

  NavigateToURL(first_url);
  for (int i = 0; i < 10; ++i) {
    Sleep(sleep_timeout_ms());
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
    if (!redirects.empty())
      break;
  }

  EXPECT_EQ(1, redirects.size());
  EXPECT_EQ(first_url.spec() + "#myanchor", redirects[0].spec());
}

// TODO(timsteele): This is disabled because our current testserver can't
// handle multiple requests in parallel, making it hang on the first request
// to /slow?60. It's unable to serve our second request for files/title2.html
// until /slow? completes, which doesn't give the desired behavior. We could
// alternatively load the second page from disk, but we would need to start
// the browser for this testcase with --process-per-tab, and I don't think
// we can do this at test-case-level granularity at the moment.
TEST_F(RedirectTest,
       DISABLED_ClientCancelledByNewNavigationAfterProvisionalLoad) {
  // We want to initiate a second navigation after the provisional load for
  // the client redirect destination has started, but before this load is
  // committed. To achieve this, we tell the browser to load a slow page,
  // which causes it to start a provisional load, and while it is waiting
  // for the response (which means it hasn't committed the load for the client
  // redirect destination page yet), we issue a new navigation request.
  scoped_refptr<HTTPTestServer> server =
    HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  GURL final_url = server->TestServerPageW(std::wstring(L"files/title2.html"));
  GURL slow = server->TestServerPageW(std::wstring(L"slow?60"));
  GURL first_url = server->TestServerPageW(
      std::wstring(L"client-redirect?") + UTF8ToWide(slow.spec()));
  std::vector<GURL> redirects;

  NavigateToURL(first_url);
  // We don't sleep here - the first navigation won't have been committed yet
  // because we told the server to wait a minute. This means the browser has
  // started it's provisional load for the client redirect destination page but
  // hasn't completed. Our time is now!
  NavigateToURL(final_url);

  std::wstring tab_title;
  std::wstring final_url_title = L"Title Of Awesomeness";
  // Wait till the final page has been loaded.
  for (int i = 0; i < 10; ++i) {
    Sleep(sleep_timeout_ms());
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->GetTabTitle(&tab_title));
    if (tab_title == final_url_title) {
      ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
      break;
    }
  }

  // Check to make sure the navigation did in fact take place and we are
  // at the expected page.
  EXPECT_EQ(final_url_title, tab_title);

  bool final_navigation_not_redirect = true;
  // Check to make sure our request for files/title2.html doesn't get flagged
  // as a client redirect from the first (/client-redirect?) page.
  for (std::vector<GURL>::iterator it = redirects.begin();
       it != redirects.end(); ++it) {
    if (final_url.spec() == it->spec()) {
      final_navigation_not_redirect = false;
      break;
    }
  }
  EXPECT_TRUE(final_navigation_not_redirect);
}
