// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>
#include <Wincrypt.h>

#include <string>

#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/ssl_test_util.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

class SSLUITest : public UITest {
 protected:
  SSLUITest() {
    dom_automation_enabled_ = true;
    EXPECT_TRUE(util_.CheckCATrusted());
  }

  TabProxy* GetActiveTabProxy() {
    scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(browser_proxy.get());
    return browser_proxy->GetActiveTab();
  }

  void NavigateTab(TabProxy* tab_proxy, const GURL& url) {
    ASSERT_TRUE(tab_proxy->NavigateToURL(url));
  }

  void AppendTab(const GURL& url) {
    scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(browser_proxy.get());
    EXPECT_TRUE(browser_proxy->AppendTab(url));
  }

  TestServer* PlainServer() {
    return new TestServer(kDocRoot);
  }

  HTTPSTestServer* GoodCertServer() {
    return new HTTPSTestServer(util_.kHostName, util_.kOKHTTPSPort,
        kDocRoot, util_.GetOKCertPath().ToWStringHack());
  }

  HTTPSTestServer* BadCertServer() {
    return new HTTPSTestServer(util_.kHostName, util_.kBadHTTPSPort,
        kDocRoot, util_.GetExpiredCertPath().ToWStringHack());
  }

 protected:
  SSLTestUtil util_;

  DISALLOW_COPY_AND_ASSIGN(SSLUITest);
};

}  // namespace

// Visits a regular page over http.
TEST_F(SSLUITest, TestHTTP) {
  scoped_ptr<TestServer> server(PlainServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), server->TestServerPageW(L"files/ssl/google.html"));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Visits a page over http which includes broken https resources (status should
// be OK).
TEST_F(SSLUITest, TestHTTPWithBrokenHTTPSResource) {
  scoped_ptr<TestServer> http_server(PlainServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());

  NavigateTab(tab.get(),
      http_server->TestServerPageW(L"files/ssl/page_with_unsafe_contents.html"));

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Visits a page over OK https:
TEST_F(SSLUITest, TestOKHTTPS) {
  scoped_ptr<HTTPSTestServer> https_server(GoodCertServer());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              https_server->TestServerPageW(L"files/ssl/google.html"));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Visits a page with https error:
TEST_F(SSLUITest, TestHTTPSExpiredCert) {
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              bad_https_server->TestServerPageW(L"files/ssl/google.html"));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::INTERSTITIAL_PAGE, page_type);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  EXPECT_TRUE(tab->TakeActionOnSSLBlockingPage(true));
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

//
// Mixed contents
//

// Visits a page with mixed content.
TEST_F(SSLUITest, TestMixedContents) {
  scoped_ptr<HTTPSTestServer> https_server(GoodCertServer());
  scoped_ptr<TestServer> http_server(PlainServer());

  // Load a page with mixed-content, the default behavior is to show the mixed
  // content.
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
      https_server->TestServerPageW(L"files/ssl/page_with_mixed_contents.html"));
  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::MIXED_CONTENT, mixed_content_state);

  // Now select the block mixed-content pref and reload the page.
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  EXPECT_TRUE(browser_proxy->SetIntPreference(prefs::kMixedContentFiltering,
                                              FilterPolicy::FILTER_ALL));
  EXPECT_TRUE(tab->Reload());

  // The image should be filtered.
  int img_width;
  EXPECT_TRUE(tab->ExecuteAndExtractInt(L"",
      L"window.domAutomationController.send(ImageWidth());",
      &img_width));
  // In order to check that the image was not loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, we assume the broken
  // image is less than 100.
  EXPECT_GT(100, img_width);

  // The state should be OK since we are not showing the resource.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // There should be one info-bar to show the mixed-content.
  int info_bar_count = 0;
  EXPECT_TRUE(tab->GetSSLInfoBarCount(&info_bar_count));
  EXPECT_EQ(1, info_bar_count);

  // Activate the link on the info-bar to show the mixed-content.
  EXPECT_TRUE(tab->ClickSSLInfoBarLink(0, true));

  // The image should show now.
  EXPECT_TRUE(tab->ExecuteAndExtractInt(L"",
      L"window.domAutomationController.send(ImageWidth());",
      &img_width));
  EXPECT_LT(100, img_width);

  // And our status should be mixed-content.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::MIXED_CONTENT, mixed_content_state);
}

// Visits a page with unsafe content and make sure that:
// - frames content is replaced with warning
// - images and scripts are filtered out entirely
TEST_F(SSLUITest, TestUnsafeContents) {
  scoped_ptr<HTTPSTestServer> good_https_server(GoodCertServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
      good_https_server->TestServerPageW(
          L"files/ssl/page_with_unsafe_contents.html"));
  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  // When the bad content is filtered, the state is expected to be
  // authenticated.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Because of cross-frame scripting restrictions, we cannot access the iframe
  // content.  So to know if the frame was loaded, we just check if a popup was
  // opened (the iframe content opens one).
  // Note: because of bug 1115868, no constrained window is opened right now.
  //       Once the bug is fixed, this will do the real check.
  int constrained_window_count = 0;
  EXPECT_TRUE(tab->GetConstrainedWindowCount(&constrained_window_count));
  EXPECT_EQ(0, constrained_window_count);

  int img_width;
  EXPECT_TRUE(tab->ExecuteAndExtractInt(L"",
      L"window.domAutomationController.send(ImageWidth());",
      &img_width));
  // In order to check that the image was not loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, we assume the broken
  // image is less than 100.
  EXPECT_GT(100, img_width);

  bool js_result = false;
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(IsFooSet());",
      &js_result));
  EXPECT_FALSE(js_result);
}

// Visits a page with mixed content loaded by JS (after the initial page load).
TEST_F(SSLUITest, TestMixedContentsLoadedFromJS) {
  scoped_ptr<HTTPSTestServer> https_server(GoodCertServer());
  scoped_ptr<TestServer> http_server(PlainServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), https_server->TestServerPageW(
      L"files/ssl/page_with_dynamic_mixed_contents.html"));
  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);
  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
                                         L"loadBadImage();",
                                         &js_result));
  EXPECT_TRUE(js_result);

  // We should now have mixed-contents.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::MIXED_CONTENT, mixed_content_state);
}

// Visits a page with an image over http.  Visits another page over https
// referencing that same image over http (hoping it is coming from the webcore
// memory cache).
TEST_F(SSLUITest, TestCachedMixedContents) {
  scoped_ptr<HTTPSTestServer> https_server(GoodCertServer());
  scoped_ptr<TestServer> http_server(PlainServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), http_server->TestServerPageW(
      L"files/ssl/page_with_mixed_contents.html"));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);
  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Load again but over SSL.  It should have mixed-contents (even though the
  // image comes from the WebCore memory cache).
  NavigateTab(tab.get(), https_server->TestServerPageW(
      L"files/ssl/page_with_mixed_contents.html"));

  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::MIXED_CONTENT, mixed_content_state);
}

// This test ensures the CN invalid status does not 'stick' to a certificate
// (see bug #1044942) and that it depends on the host-name.
// TODO(jcampan): this test is flacky and fails sometimes (bug #1065095)
TEST_F(SSLUITest, DISABLED_TestCNInvalidStickiness) {
  const std::string kLocalHost = "localhost";
  scoped_ptr<HTTPSTestServer> https_server(
    new HTTPSTestServer(kLocalHost, util_.kOKHTTPSPort,
    kDocRoot, util_.GetOKCertPath().ToWStringHack()));

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), https_server->TestServerPageW(
      L"files/ssl/google.html"));

  // We get an interstitial page as a result.
  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::INTERSTITIAL_PAGE, page_type);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_COMMON_NAME_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // We proceed through the interstitial page.
  EXPECT_TRUE(tab->TakeActionOnSSLBlockingPage(true));
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);

  // Now we try again with the right host name this time.

  // Let's change the host-name in the url.
  GURL url = https_server->TestServerPageW(L"files/ssl/google.html");
  std::string::size_type hostname_index = url.spec().find(kLocalHost);
  ASSERT_TRUE(hostname_index != std::string::npos);  // Test sanity check.
  std::string new_url;
  new_url.append(url.spec().substr(0, hostname_index));
  new_url.append(util_.kHostName);
  new_url.append(url.spec().substr(hostname_index + kLocalHost.size()));

  NavigateTab(tab.get(), GURL(new_url));

  // Security state should be OK.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Now try again the broken one to make sure it is still broken.
  NavigateTab(tab.get(), https_server->TestServerPageW(
      L"files/ssl/google.html"));

  EXPECT_TRUE(tab->GetPageType(&page_type));
  // Since we OKed the interstitial last time, we get right to the page.
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_COMMON_NAME_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Test that navigating to a #ref does not change a bad security state.
TEST_F(SSLUITest, TestRefNavigation) {
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              bad_https_server->TestServerPageW(L"files/ssl/page_with_refs.html"));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(page_type, NavigationEntry::INTERSTITIAL_PAGE);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID, cert_status);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  EXPECT_TRUE(tab->TakeActionOnSSLBlockingPage(true));
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Now navigate to a ref in the page.
  NavigateTab(tab.get(),
      bad_https_server->TestServerPageW(L"files/ssl/page_with_refs.html#jp"));
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Tests that closing a page that has a unsafe pop-up does not crash the browser
// (bug #1966).
// Disabled because flaky (bug #2136).
TEST_F(SSLUITest, DISABLED_TestCloseTabWithUnsafePopup) {
  scoped_ptr<TestServer> http_server(PlainServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              http_server->TestServerPageW(
                  L"files/ssl/page_with_unsafe_popup.html"));

  int popup_count = 0;
  EXPECT_TRUE(tab->GetConstrainedWindowCount(&popup_count));
  EXPECT_EQ(1, popup_count);

  // Let's add another tab to make sure the browser does not exit when we close
  // the first tab.
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  browser_proxy->AppendTab(
      http_server->TestServerPageW(L"files/ssl/google.html"));

  // Close the first tab.
  tab->Close();
}

// Visit a page over bad https that is a redirect to a page with good https.
TEST_F(SSLUITest, TestRedirectBadToGoodHTTPS) {
  scoped_ptr<HTTPSTestServer> good_https_server(GoodCertServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  GURL url1 = bad_https_server->TestServerPageW(L"server-redirect?");
  GURL url2 = good_https_server->TestServerPageW(L"files/ssl/google.html");
  NavigateTab(tab.get(), GURL(url1.spec() + url2.spec()));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(page_type, NavigationEntry::INTERSTITIAL_PAGE);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID, cert_status);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  EXPECT_TRUE(tab->TakeActionOnSSLBlockingPage(true));
  // We have been redirected to the good page.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0,
            cert_status & net::CERT_STATUS_ALL_ERRORS);  // No errors expected.
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Visit a page over good https that is a redirect to a page with bad https.
TEST_F(SSLUITest, TestRedirectGoodToBadHTTPS) {
  scoped_ptr<HTTPSTestServer> good_https_server(GoodCertServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  GURL url1 = good_https_server->TestServerPageW(L"server-redirect?");
  GURL url2 = bad_https_server->TestServerPageW(L"files/ssl/google.html");
  NavigateTab(tab.get(), GURL(url1.spec() + url2.spec()));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(page_type, NavigationEntry::INTERSTITIAL_PAGE);

  EXPECT_TRUE(tab->TakeActionOnSSLBlockingPage(true));

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Visit a page over http that is a redirect to a page with https (good and
// bad).
TEST_F(SSLUITest, TestRedirectHTTPToHTTPS) {
  scoped_ptr<TestServer> http_server(PlainServer());
  scoped_ptr<HTTPSTestServer> good_https_server(GoodCertServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  // HTTP redirects to good HTTPS.
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  GURL http_url = http_server->TestServerPageW(L"server-redirect?");
  GURL good_https_url =
      good_https_server->TestServerPageW(L"files/ssl/google.html");
  NavigateTab(tab.get(), GURL(http_url.spec() + good_https_url.spec()));

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // HTTP redirects to bad HTTPS.
  GURL bad_https_url =
      bad_https_server->TestServerPageW(L"files/ssl/google.html");
  NavigateTab(tab.get(), GURL(http_url.spec() + bad_https_url.spec()));

  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(page_type, NavigationEntry::INTERSTITIAL_PAGE);

  // Continue on the interstitial.
  EXPECT_TRUE(tab->TakeActionOnSSLBlockingPage(true));

  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Visit a page over https that is a redirect to a page with http (to make sure
// we don't keep the secure state).
TEST_F(SSLUITest, TestRedirectHTTPSToHTTP) {
  scoped_ptr<TestServer> http_server(PlainServer());
  scoped_ptr<HTTPSTestServer> https_server(GoodCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  GURL https_url = https_server->TestServerPageW(L"server-redirect?");
  GURL http_url = http_server->TestServerPageW(L"files/ssl/google.html");
  NavigateTab(tab.get(), GURL(https_url.spec() + http_url.spec()));

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Visits a page to which we could not connect (bad port) over http and https
// and make sure the security style is correct.
TEST_F(SSLUITest, TestConnectToBadPort) {
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());

  GURL http_url("http://localhost:17");
  NavigateTab(tab.get(), http_url);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Same thing over HTTPS.
  GURL https_url("https://localhost:17");
  NavigateTab(tab.get(), https_url);

  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

//
// Frame navigation
//

// From a good HTTPS top frame:
// - navigate to an OK HTTPS frame
// - navigate to a bad HTTPS (expect unsafe content and filtered frame), then
//   back
// - navigate to HTTP (expect mixed content), then back
TEST_F(SSLUITest, TestGoodFrameNavigation) {
  scoped_ptr<TestServer> http_server(PlainServer());
  scoped_ptr<HTTPSTestServer> good_https_server(GoodCertServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              good_https_server->TestServerPageW(L"files/ssl/top_frame.html"));

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  bool success = false;
  // Now navigate inside the frame.
  int64 last_nav_time = 0;
  EXPECT_TRUE(tab->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(tab->WaitForNavigation(last_nav_time));

  // We should still be fine.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Now let's hit a bad page.
  EXPECT_TRUE(tab->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(clickLink('badHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(tab->WaitForNavigation(last_nav_time));

  // The security style should still be secure.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // And the frame should be blocked.
  bool is_content_evil = true;
  std::wstring content_frame_xpath(L"html/frameset/frame[2]");
  std::wstring is_frame_evil_js(
      L"window.domAutomationController"
      L".send(document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(tab->ExecuteAndExtractBool(content_frame_xpath,
                                         is_frame_evil_js,
                                         &is_content_evil));
  EXPECT_FALSE(is_content_evil);

  // Now go back, our state should return to OK.
  EXPECT_TRUE(tab->GoBack());
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Navigate to a page served over HTTP.
  EXPECT_TRUE(tab->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(clickLink('HTTPLink'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(tab->WaitForNavigation(last_nav_time));

  // Our state should be mixed-content.
  // Status should be "contains bad contents".
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::MIXED_CONTENT, mixed_content_state);

  // Go back, our state should be back to OK.
  EXPECT_TRUE(tab->GoBack());
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// From a bad HTTPS top frame:
// - navigate to an OK HTTPS frame (expected to be still authentication broken).
TEST_F(SSLUITest, TestBadFrameNavigation) {
  scoped_ptr<HTTPSTestServer> good_https_server(GoodCertServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              bad_https_server->TestServerPageW(L"files/ssl/top_frame.html"));

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Continue on the interstitial.
  EXPECT_TRUE(tab->TakeActionOnSSLBlockingPage(true));

  // Navigate to a good frame.
  bool success = false;
  int64 last_nav_time = 0;
  EXPECT_TRUE(tab->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(tab->WaitForNavigation(last_nav_time));

  // We should still be authentication broken.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
TEST_F(SSLUITest, TestUnauthenticatedFrameNavigation) {
  scoped_ptr<TestServer> http_server(PlainServer());
  scoped_ptr<HTTPSTestServer> good_https_server(GoodCertServer());
  scoped_ptr<HTTPSTestServer> bad_https_server(BadCertServer());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              http_server->TestServerPageW(L"files/ssl/top_frame.html"));

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Now navigate inside the frame to a secure HTTPS frame.
  bool success = false;
  int64 last_nav_time = 0;
  EXPECT_TRUE(tab->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(tab->WaitForNavigation(last_nav_time));

  // We should still be unauthenticated.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // Now navigate to a bad HTTPS frame.
  EXPECT_TRUE(tab->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(clickLink('badHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(tab->WaitForNavigation(last_nav_time));

  // State should not have changed.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
  EXPECT_EQ(0, cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);

  // And the frame should have been blocked (see bug #2316).
  bool is_content_evil = true;
  std::wstring content_frame_xpath(L"html/frameset/frame[2]");
  std::wstring is_frame_evil_js(
      L"window.domAutomationController"
      L".send(document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(tab->ExecuteAndExtractBool(content_frame_xpath,
                                         is_frame_evil_js,
                                         &is_content_evil));
  EXPECT_FALSE(is_content_evil);
}


// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest mixed in synchronous mode.

// XMLHttpRequest mixed in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.
