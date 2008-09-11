// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>
#include <Wincrypt.h>

#include <string>

#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";
const char kHostName[] = "127.0.0.1";

const int kOKHTTPSPort = 9443;
const int kBadHTTPSPort = 9666;

// The issuer name of the cert that should be trusted for the test to work.
const wchar_t kCertIssuerName[] = L"Test CA";

class SSLUITest : public UITest {
 protected:
  SSLUITest() {
    CheckCATrusted();
    dom_automation_enabled_ = true;
    PathService::Get(base::DIR_SOURCE_ROOT, &cert_dir_);
    cert_dir_ += L"/chrome/test/data/ssl/certs/";
    std::replace(cert_dir_.begin(), cert_dir_.end(),
                 L'/', file_util::kPathSeparator);
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

  std::wstring GetOKCertPath() {
    std::wstring path(cert_dir_);
    file_util::AppendToPath(&path, L"ok_cert.pem");
    return path;
  }

  std::wstring GetInvalidCertPath() {
    std::wstring path(cert_dir_);
    file_util::AppendToPath(&path, L"invalid_cert.pem");
    return path;
  }

  std::wstring GetExpiredCertPath() {
    std::wstring path(cert_dir_);
    file_util::AppendToPath(&path, L"expired_cert.pem");
    return path;
  }

 private:
  void CheckCATrusted() {
    HCERTSTORE cert_store = CertOpenSystemStore(NULL, L"ROOT");
    if (!cert_store) {
      FAIL() << " could not open trusted root CA store";
      return;
    }
    PCCERT_CONTEXT cert =
        CertFindCertificateInStore(cert_store,
                                   X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                   0,
                                   CERT_FIND_ISSUER_STR,
                                   kCertIssuerName,
                                   NULL);
    if (cert)
      CertFreeCertificateContext(cert);
    CertCloseStore(cert_store, 0);

    if (!cert) {
      FAIL() << " TEST CONFIGURATION ERROR: you need to import the test ca "
          "certificate to your trusted roots for this test to work. For more "
          "info visit:\n"
          "http://wiki.corp.google.com/twiki/bin/view/Main/ChromeUnitUITests\n";
    }
  }

  std::wstring cert_dir_;
};

}  // namespace

// Visits a regular page over http.
TEST_F(SSLUITest, TestHTTP) {
  TestServer server(kDocRoot);

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), server.TestServerPageW(L"files/ssl/google.html"));

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
  TestServer http_server(kDocRoot);
  HTTPSTestServer httpsServer(kHostName, kBadHTTPSPort,
                              kDocRoot, GetExpiredCertPath());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());

  NavigateTab(tab.get(),
      http_server.TestServerPageW(L"files/ssl/page_with_unsafe_contents.html"));

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
  HTTPSTestServer https_server(kHostName, kOKHTTPSPort,
                               kDocRoot, GetOKCertPath());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              https_server.TestServerPageW(L"files/ssl/google.html"));

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
  HTTPSTestServer https_server(kHostName, kBadHTTPSPort,
                               kDocRoot, GetExpiredCertPath());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              https_server.TestServerPageW(L"files/ssl/google.html"));

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
  HTTPSTestServer https_server(kHostName, kOKHTTPSPort,
                               kDocRoot, GetOKCertPath());
  TestServer http_server(kDocRoot);

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
      https_server.TestServerPageW(L"files/ssl/page_with_mixed_contents.html"));
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
}

// Visits a page with unsafe content and make sure that:
// - frames content is replaced with warning
// - images and scripts are filtered out entirely
TEST_F(SSLUITest, TestUnsafeContents) {
  HTTPSTestServer good_https_server(kHostName, kOKHTTPSPort,
                                    kDocRoot, GetOKCertPath());
  HTTPSTestServer bad_https_server(kHostName, kBadHTTPSPort,
                                   kDocRoot, GetExpiredCertPath());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
      good_https_server.TestServerPageW(
          L"files/ssl/page_with_unsafe_contents.html"));
  NavigationEntry::PageType page_type;
  EXPECT_TRUE(tab->GetPageType(&page_type));
  EXPECT_EQ(NavigationEntry::NORMAL_PAGE, page_type);

  SecurityStyle security_style;
  int cert_status;
  int mixed_content_state;
  // When the bad content is filtered, the state is expected to be
  // unauthenticated.
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, security_style);
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
      L"javascript:void(window.domAutomationController)"
      L".send(ImageWidth());",
      &img_width));
  // In order to check that the image was not loaded, we check its width.
  // The actual image (Google logo is 114 pixels wide), we assume the broken
  // image is less than 100.
  EXPECT_GT(100, img_width);

  bool js_result = false;
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"javascript:void(window.domAutomationController)"
      L".send(IsFooSet());",
      &js_result));
  EXPECT_FALSE(js_result);
}

// Visits a page with mixed content loaded by JS (after the initial page load).
TEST_F(SSLUITest, TestMixedContentsLoadedFromJS) {
  HTTPSTestServer https_server(kHostName, kOKHTTPSPort,
                               kDocRoot, GetOKCertPath());
  TestServer http_server(kDocRoot);

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), https_server.TestServerPageW(
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
                                         L"javascript:loadBadImage();",
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
  HTTPSTestServer https_server(kHostName, kOKHTTPSPort,
                               kDocRoot, GetOKCertPath());
  TestServer http_server(kDocRoot);

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), http_server.TestServerPageW(
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
  NavigateTab(tab.get(), https_server.TestServerPageW(
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
// TODO (jcampan): this test is flacky and fails sometimes (bug #1065095)
TEST_F(SSLUITest, DISABLED_TestCNInvalidStickiness) {
  const std::string kLocalHost = "localhost";
  HTTPSTestServer https_server(kLocalHost, kOKHTTPSPort,
                               kDocRoot, GetOKCertPath());

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(), https_server.TestServerPageW(
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
  GURL url = https_server.TestServerPageW(L"files/ssl/google.html");
  std::string::size_type hostname_index = url.spec().find(kLocalHost);
  ASSERT_TRUE(hostname_index != std::string::npos);  // Test sanity check.
  std::string new_url;
  new_url.append(url.spec().substr(0, hostname_index));
  new_url.append(kHostName);
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
  NavigateTab(tab.get(), https_server.TestServerPageW(
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
  HTTPSTestServer https_server(kHostName, kBadHTTPSPort,
                               kDocRoot, GetExpiredCertPath());
  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              https_server.TestServerPageW(L"files/ssl/page_with_refs.html"));

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
      https_server.TestServerPageW(L"files/ssl/page_with_refs.html#jp"));
  EXPECT_TRUE(tab->GetSecurityState(&security_style, &cert_status,
                                    &mixed_content_state));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, security_style);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            cert_status & net::CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(NavigationEntry::SSLStatus::NORMAL_CONTENT, mixed_content_state);
}

// Tests that closing a page that has a unsafe pop-up does not crash the browser
// (bug #1966).
TEST_F(SSLUITest, TestCloseTabWithUnsafePopup) {
  TestServer http_server(kDocRoot);
  HTTPSTestServer bad_https_server(kHostName, kBadHTTPSPort,
                                   kDocRoot, GetExpiredCertPath());

  scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  NavigateTab(tab.get(),
              http_server.TestServerPageW(
                  L"files/ssl/page_with_unsafe_popup.html"));

  int popup_count = 0;
  EXPECT_TRUE(tab->GetConstrainedWindowCount(&popup_count));
  EXPECT_EQ(1, popup_count);

  // Let's add another tab to make sure the browser does not exit when we close
  // the first tab.
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  EXPECT_TRUE(browser_proxy.get());
  browser_proxy->AppendTab(
      http_server.TestServerPageW(L"files/ssl/google.html"));

  // Close the first tab.
  tab->Close();
}

// TODO (jcampan): more tests to do below.

// Visit a page over bad https that is a redirect to a page with good https.

// Visit a page over good https that is a redirect to a page with bad https.

// Visit a page over http that is a redirect to a page with https (good and
// bad).

// Visit a page over https that is a redirect to a page with http.

// Visit a page over https that contains a frame with a redirect.

// Visits a page to which we could not connect (bad port) over http and https

// XMLHttpRequest mixed in synchronous mode.

// XMLHttpRequest mixed in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.

//
// Frame navigation
//

// Navigate to broken frame and back.

