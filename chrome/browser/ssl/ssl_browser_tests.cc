// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

const wchar_t kDocRoot[] = L"chrome/test/data";

class SSLUITest : public InProcessBrowserTest {
 public:
  SSLUITest() {
    EnableDOMAutomation();
  }

  scoped_refptr<HTTPTestServer> PlainServer() {
    return HTTPTestServer::CreateServer(kDocRoot, NULL);
  }

  scoped_refptr<HTTPSTestServer> GoodCertServer() {
    return HTTPSTestServer::CreateGoodServer(kDocRoot);
  }

  scoped_refptr<HTTPSTestServer> BadCertServer() {
    return HTTPSTestServer::CreateExpiredServer(kDocRoot);
  }

  void CheckAuthenticatedState(TabContents* tab,
                               bool mixed_content,
                               bool unsafe_content) {
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(NavigationEntry::NORMAL_PAGE, entry->page_type());
    EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, entry->ssl().security_style());
    EXPECT_EQ(0, entry->ssl().cert_status() & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_EQ(mixed_content, entry->ssl().has_mixed_content());
    EXPECT_EQ(unsafe_content, entry->ssl().has_unsafe_content());
  }

  void CheckUnauthenticatedState(TabContents* tab) {
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(NavigationEntry::NORMAL_PAGE, entry->page_type());
    EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, entry->ssl().security_style());
    EXPECT_EQ(0, entry->ssl().cert_status() & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(entry->ssl().has_mixed_content());
    EXPECT_FALSE(entry->ssl().has_unsafe_content());
  }

  void CheckAuthenticationBrokenState(TabContents* tab,
                                      int error,
                                      bool interstitial) {
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    ASSERT_TRUE(entry);
    EXPECT_EQ(interstitial ? NavigationEntry::INTERSTITIAL_PAGE :
                             NavigationEntry::NORMAL_PAGE,
              entry->page_type());
    EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN,
              entry->ssl().security_style());
    EXPECT_EQ(error, entry->ssl().cert_status() & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(entry->ssl().has_mixed_content());
    EXPECT_FALSE(entry->ssl().has_unsafe_content());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLUITest);
};

// Visits a regular page over http.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTP) {
  scoped_refptr<HTTPTestServer> server = PlainServer();

  ui_test_utils::NavigateToURL(browser(),
      server->TestServerPageW(L"files/ssl/google.html"));

  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

// Visits a page over http which includes broken https resources (status should
// be OK).
// TODO(jcampan): test that bad HTTPS content is blocked (otherwise we'll give
//                the secure cookies away!).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPWithBrokenHTTPSResource) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  ui_test_utils::NavigateToURL(browser(), http_server->TestServerPageW(
      L"files/ssl/page_with_unsafe_contents.html"));

  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

// Visits a page over OK https:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestOKHTTPS) {
  scoped_refptr<HTTPSTestServer> https_server = GoodCertServer();

  ui_test_utils::NavigateToURL(browser(),
      https_server->TestServerPageW(L"files/ssl/google.html"));

  CheckAuthenticatedState(browser()->GetSelectedTabContents(),
                          false, false);  // No mixed/unsafe content.
}

// Visits a page with https error and proceed:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndProceed) {
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  ui_test_utils::NavigateToURL(browser(),
      bad_https_server->TestServerPageW(L"files/ssl/google.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 true);  // Interstitial showing

  // Proceed through the interstitial.
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->Proceed();
  // Wait for the navigation to be done.
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 false);  // No interstitial showing
}

// Visits a page with https error and don't proceed (and ensure we can still
// navigate at that point):
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndDontProceed) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  // First navigate to an OK page.
  ui_test_utils::NavigateToURL(browser(), good_https_server->TestServerPageW(
      L"files/ssl/google.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  NavigationEntry* entry = tab->controller().GetActiveEntry();
  ASSERT_TRUE(entry);

  GURL cross_site_url =
      bad_https_server->TestServerPageW(L"files/ssl/google.html");
  // Change the host name from 127.0.0.1 to localhost so it triggers a
  // cross-site navigation so we can test http://crbug.com/5800 is gone.
  ASSERT_EQ("127.0.0.1", cross_site_url.host());
  GURL::Replacements replacements;
  std::string new_host("localhost");
  replacements.SetHostStr(new_host);
  cross_site_url = cross_site_url.ReplaceComponents(replacements);

  // Now go to a bad HTTPS page.
  ui_test_utils::NavigateToURL(browser(), cross_site_url);

  // An interstitial should be showing.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 true);  // Interstitial showing.

  // Simulate user clicking "Take me back".
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->DontProceed();

  // We should be back to the original good page.
  CheckAuthenticatedState(tab, false, false);

  // Try to navigate to a new page. (to make sure bug 5800 is fixed).
  ui_test_utils::NavigateToURL(browser(),
      http_server->TestServerPageW(L"files/ssl/google.html"));
  CheckUnauthenticatedState(tab);
}

//
// Mixed contents
//

// Visits a page with mixed content.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestMixedContents) {
  scoped_refptr<HTTPSTestServer> https_server = GoodCertServer();
  scoped_refptr<HTTPTestServer> http_server = PlainServer();

  // Load a page with mixed-content, the default behavior is to show the mixed
  // content.
  ui_test_utils::NavigateToURL(browser(), https_server->TestServerPageW(
      L"files/ssl/page_with_mixed_contents.html"));

  CheckAuthenticatedState(browser()->GetSelectedTabContents(),
                          true /* mixed-content */, false);
}

// Visits a page with an http script that tries to suppress our mixed content
// warnings by randomize location.hash.
// Based on http://crbug.com/8706
IN_PROC_BROWSER_TEST_F(SSLUITest, TestMixedContentsRandomizeHash) {
  scoped_refptr<HTTPSTestServer> https_server = GoodCertServer();
  scoped_refptr<HTTPTestServer> http_server = PlainServer();

  ui_test_utils::NavigateToURL(browser(), https_server->TestServerPageW(
      L"files/ssl/page_with_http_script.html"));

  CheckAuthenticatedState(browser()->GetSelectedTabContents(),
                          true /* mixed-content */, false);
}

// Visits a page with unsafe content and make sure that:
// - frames content is replaced with warning
// - images and scripts are filtered out entirely
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContents) {
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  ui_test_utils::NavigateToURL(browser(), good_https_server->TestServerPageW(
      L"files/ssl/page_with_unsafe_contents.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  // When the bad content is filtered, the state is expected to be
  // authenticated.
  CheckAuthenticatedState(tab, false, false);

  // Because of cross-frame scripting restrictions, we cannot access the iframe
  // content.  So to know if the frame was loaded, we just check if a popup was
  // opened (the iframe content opens one).
  // Note: because of bug 1115868, no constrained window is opened right now.
  //       Once the bug is fixed, this will do the real check.
  EXPECT_EQ(0, static_cast<int>(tab->constrained_window_count()));

  int img_width;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(ImageWidth());", &img_width));
  // In order to check that the image was not loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, we assume the broken
  // image is less than 100.
  EXPECT_GT(100, img_width);

  bool js_result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(IsFooSet());", &js_result));
  EXPECT_FALSE(js_result);
}

// Visits a page with mixed content loaded by JS (after the initial page load).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestMixedContentsLoadedFromJS) {
  scoped_refptr<HTTPSTestServer> https_server = GoodCertServer();
  scoped_refptr<HTTPTestServer> http_server = PlainServer();

  ui_test_utils::NavigateToURL(browser(), https_server->TestServerPageW(
      L"files/ssl/page_with_dynamic_mixed_contents.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticatedState(tab, false, false);

  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"loadBadImage();", &js_result));
  EXPECT_TRUE(js_result);

  // We should now have mixed-contents.
  CheckAuthenticatedState(tab, true /* mixed-content */, false);
}

// Visits two pages from the same origin: one with mixed content and one
// without.  The test checks that we propagate the mixed content state from one
// to the other.
// TODO(jcampan): http://crbug.com/15072 this test fails.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestMixedContentsTwoTabs) {
  scoped_refptr<HTTPSTestServer> https_server = GoodCertServer();
  scoped_refptr<HTTPTestServer> http_server = PlainServer();

  ui_test_utils::NavigateToURL(browser(),
      https_server->TestServerPageW(L"files/ssl/blank_page.html"));

  TabContents* tab1 = browser()->GetSelectedTabContents();

  // This tab should be fine.
  CheckAuthenticatedState(tab1, false, false);

  // Create a new tab.
  GURL url =
      https_server->TestServerPageW(L"files/ssl/page_with_http_script.html");
  TabContents* tab2 = browser()->AddTabWithURL(url,
                                               GURL(),
                                               PageTransition::TYPED, true, 0,
                                               false, NULL);
  ui_test_utils::WaitForNavigation(&(tab2->controller()));

  // The new tab has mixed content.
  CheckAuthenticatedState(tab2, true /* mixed-content */, false);

  // Which means the origin for the first tab has also been contaminated with
  // mixed content.
  CheckAuthenticatedState(tab1, true /* mixed-content */, false);
}

// Visits a page with an image over http.  Visits another page over https
// referencing that same image over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestCachedMixedContents) {
  scoped_refptr<HTTPSTestServer> https_server = GoodCertServer();
  scoped_refptr<HTTPTestServer> http_server = PlainServer();

  ui_test_utils::NavigateToURL(browser(), http_server->TestServerPageW(
      L"files/ssl/page_with_mixed_contents.html"));
  TabContents* tab = browser()->GetSelectedTabContents();
  CheckUnauthenticatedState(tab);

  // Load again but over SSL.  It should have mixed-contents (even though the
  // image comes from the WebCore memory cache).
  ui_test_utils::NavigateToURL(browser(), https_server->TestServerPageW(
      L"files/ssl/page_with_mixed_contents.html"));
  CheckAuthenticatedState(tab, true /* mixed-content */, false);
}

// This test ensures the CN invalid status does not 'stick' to a certificate
// (see bug #1044942) and that it depends on the host-name.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestCNInvalidStickiness) {
  const std::string kLocalHost = "localhost";
  scoped_refptr<HTTPSTestServer> https_server =
      HTTPSTestServer::CreateMismatchedServer(kDocRoot);
  ASSERT_TRUE(NULL != https_server.get());

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  ui_test_utils::NavigateToURL(browser(),
      https_server->TestServerPageW(L"files/ssl/google.html"));

  // We get an interstitial page as a result.
  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 true);  // Interstitial showing.

  // We proceed through the interstitial page.
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->Proceed();
  // Wait for the navigation to be done.
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 false);  // No interstitial showing.

  // Now we try again with the right host name this time.

  // Let's change the host-name in the url.
  GURL url = https_server->TestServerPageW(L"files/ssl/google.html");
  std::string::size_type hostname_index = url.spec().find(kLocalHost);
  ASSERT_TRUE(hostname_index != std::string::npos);  // Test sanity check.
  std::string new_url;
  new_url.append(url.spec().substr(0, hostname_index));
  new_url.append(net::TestServerLauncher::kHostName);
  new_url.append(url.spec().substr(hostname_index + kLocalHost.size()));

  ui_test_utils::NavigateToURL(browser(), GURL(new_url));

  // Security state should be OK.
  CheckAuthenticatedState(tab, false, false);

  // Now try again the broken one to make sure it is still broken.
  ui_test_utils::NavigateToURL(browser(),
      https_server->TestServerPageW(L"files/ssl/google.html"));

  // Since we OKed the interstitial last time, we get right to the page.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 false);  // No interstitial showing.
}

// Test that navigating to a #ref does not change a bad security state.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRefNavigation) {
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  ui_test_utils::NavigateToURL(browser(),
      bad_https_server->TestServerPageW(L"files/ssl/page_with_refs.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 true);  // Interstitial showing.

  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->Proceed();
  // Wait for the navigation to be done.
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 false);  // No interstitial showing.

  // Now navigate to a ref in the page, the security state should not have
  // changed.
  ui_test_utils::NavigateToURL(browser(),
      bad_https_server->TestServerPageW(L"files/ssl/page_with_refs.html#jp"));

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 false);  // No interstitial showing.
}

// Tests that closing a page that has a unsafe pop-up does not crash the
// browser (bug #1966).
// TODO(jcampan): http://crbug.com/2136 disabled because the popup is not
//                opened as it is not initiated by a user gesture.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCloseTabWithUnsafePopup) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  ui_test_utils::NavigateToURL(browser(), http_server->TestServerPageW(
      L"files/ssl/page_with_unsafe_popup.html"));

  TabContents* tab1 = browser()->GetSelectedTabContents();
  // It is probably overkill to add a notification for a popup-opening, let's
  // just poll.
  for (int i = 0; i < 10; i++) {
    if (static_cast<int>(tab1->constrained_window_count()) > 0)
      break;
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new MessageLoop::QuitTask(), 1000);
    ui_test_utils::RunMessageLoop();
  }
  ASSERT_EQ(1, static_cast<int>(tab1->constrained_window_count()));

  // Let's add another tab to make sure the browser does not exit when we close
  // the first tab.
  GURL url = http_server->TestServerPageW(L"files/ssl/google.html");
  TabContents* tab2 = browser()->AddTabWithURL(url,
                                               GURL(),
                                               PageTransition::TYPED,
                                               true, 0, false, NULL);
  ui_test_utils::WaitForNavigation(&(tab2->controller()));

  // Close the first tab.
  browser()->CloseTabContents(tab1);
}

// Visit a page over bad https that is a redirect to a page with good https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectBadToGoodHTTPS) {
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  GURL url1 = bad_https_server->TestServerPageW(L"server-redirect?");
  GURL url2 = good_https_server->TestServerPageW(L"files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),  GURL(url1.spec() + url2.spec()));

  TabContents* tab = browser()->GetSelectedTabContents();

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 true);  // Interstitial showing.

  // We proceed through the interstitial page.
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->Proceed();
  // Wait for the navigation to be done.
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  // We have been redirected to the good page.
  CheckAuthenticatedState(tab, false, false);  // No mixed/unsafe content.
}

// Visit a page over good https that is a redirect to a page with bad https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectGoodToBadHTTPS) {
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  GURL url1 = good_https_server->TestServerPageW(L"server-redirect?");
  GURL url2 = bad_https_server->TestServerPageW(L"files/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(),  GURL(url1.spec() + url2.spec()));

  TabContents* tab = browser()->GetSelectedTabContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 true);  // Interstitial showing.

  // We proceed through the interstitial page.
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->Proceed();
  // Wait for the navigation to be done.
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 false);  // No interstitial showing.
}

// Visit a page over http that is a redirect to a page with good HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToGoodHTTPS) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();

  TabContents* tab = browser()->GetSelectedTabContents();

  // HTTP redirects to good HTTPS.
  GURL http_url = http_server->TestServerPageW(L"server-redirect?");
  GURL good_https_url =
      good_https_server->TestServerPageW(L"files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + good_https_url.spec()));
  CheckAuthenticatedState(tab, false, false);  // No mixed/unsafe content.
}

// Visit a page over http that is a redirect to a page with bad HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToBadHTTPS) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  TabContents* tab = browser()->GetSelectedTabContents();

  GURL http_url = http_server->TestServerPageW(L"server-redirect?");
  GURL bad_https_url =
      bad_https_server->TestServerPageW(L"files/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + bad_https_url.spec()));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 true);  // Interstitial showing.

  // Continue on the interstitial.
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->Proceed();
  // Wait for the navigation to be done.
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 false);  // No interstitial showing.
}

// Visit a page over https that is a redirect to a page with http (to make sure
// we don't keep the secure state).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPSToHTTP) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> https_server = GoodCertServer();

  GURL https_url = https_server->TestServerPageW(L"server-redirect?");
  GURL http_url = http_server->TestServerPageW(L"files/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(https_url.spec() + http_url.spec()));
  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

// Visits a page to which we could not connect (bad port) over http and https
// and make sure the security style is correct.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestConnectToBadPort) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://localhost:17"));
  CheckUnauthenticatedState(browser()->GetSelectedTabContents());

  // Same thing over HTTPS.
  ui_test_utils::NavigateToURL(browser(), GURL("https://localhost:17"));
  CheckUnauthenticatedState(browser()->GetSelectedTabContents());
}

//
// Frame navigation
//

// From a good HTTPS top frame:
// - navigate to an OK HTTPS frame
// - navigate to a bad HTTPS (expect unsafe content and filtered frame), then
//   back
// - navigate to HTTP (expect mixed content), then back
IN_PROC_BROWSER_TEST_F(SSLUITest, TestGoodFrameNavigation) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::NavigateToURL(
      browser(),
      good_https_server->TestServerPageW(L"files/ssl/top_frame.html"));

  CheckAuthenticatedState(tab, false, false);

  bool success = false;
  // Now navigate inside the frame.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // We should still be fine.
  CheckAuthenticatedState(tab, false, false);

  // Now let's hit a bad page.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(clickLink('badHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // The security style should still be secure.
  CheckAuthenticatedState(tab, false, false);

  // And the frame should be blocked.
  bool is_content_evil = true;
  std::wstring content_frame_xpath(L"html/frameset/frame[2]");
  std::wstring is_frame_evil_js(
      L"window.domAutomationController"
      L".send(document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(),
      content_frame_xpath,
      is_frame_evil_js,
      &is_content_evil));
  EXPECT_FALSE(is_content_evil);

  // Now go back, our state should still be OK.
  tab->controller().GoBack();
  ui_test_utils::WaitForNavigation(&tab->controller());
  CheckAuthenticatedState(tab, false, false);

  // Navigate to a page served over HTTP.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(),
      L"",
      L"window.domAutomationController.send(clickLink('HTTPLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // Our state should be mixed-content.
  CheckAuthenticatedState(tab, true, false);

  // Go back, our state should be unchanged.
  tab->controller().GoBack();
  ui_test_utils::WaitForNavigation(&tab->controller());
  CheckAuthenticatedState(tab, true, false);
}

// From a bad HTTPS top frame:
// - navigate to an OK HTTPS frame (expected to be still authentication broken).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadFrameNavigation) {
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::NavigateToURL(
      browser(),
      bad_https_server->TestServerPageW(L"files/ssl/top_frame.html"));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 true);  // Interstitial showing

  // Continue on the interstitial.
  InterstitialPage* interstitial_page = tab->interstitial_page();
  ASSERT_TRUE(interstitial_page);
  interstitial_page->Proceed();
  ui_test_utils::WaitForNavigation(&(tab->controller()));

  // Navigate to a good frame.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(),
      L"",
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // We should still be authentication broken.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID, false);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnauthenticatedFrameNavigation) {
  scoped_refptr<HTTPTestServer> http_server = PlainServer();
  scoped_refptr<HTTPSTestServer> good_https_server = GoodCertServer();
  scoped_refptr<HTTPSTestServer> bad_https_server = BadCertServer();

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::NavigateToURL(
      browser(),
      http_server->TestServerPageW(L"files/ssl/top_frame.html"));
  CheckUnauthenticatedState(tab);

  // Now navigate inside the frame to a secure HTTPS frame.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"",
      L"window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // We should still be unauthenticated.
  CheckUnauthenticatedState(tab);

  // Now navigate to a bad HTTPS frame.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(),
      L"",
      L"window.domAutomationController.send(clickLink('badHTTPSLink'));",
      &success));
  EXPECT_TRUE(success);
  ui_test_utils::WaitForNavigation(&tab->controller());

  // State should not have changed.
  CheckUnauthenticatedState(tab);

  // And the frame should have been blocked (see bug #2316).
  bool is_content_evil = true;
  std::wstring content_frame_xpath(L"html/frameset/frame[2]");
  std::wstring is_frame_evil_js(
      L"window.domAutomationController"
      L".send(document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), content_frame_xpath, is_frame_evil_js,
      &is_content_evil));
  EXPECT_FALSE(is_content_evil);
}

// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest mixed in synchronous mode.

// XMLHttpRequest mixed in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.
