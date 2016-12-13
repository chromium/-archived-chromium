// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/extensions/extension_shelf.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

// Looks for an ExtensionHost whose URL has the given path component (including
// leading slash).  Also verifies that the expected number of hosts are loaded.
static ExtensionHost* FindHostWithPath(ExtensionProcessManager* manager,
                                       const std::string& path,
                                       int expected_hosts) {
  ExtensionHost* host = NULL;
  int num_hosts = 0;
  for (ExtensionProcessManager::const_iterator iter = manager->begin();
       iter != manager->end(); ++iter) {
    if ((*iter)->GetURL().path() == path) {
      EXPECT_FALSE(host);
      host = *iter;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return host;
}

// Tests that toolstrips initializes properly and can run basic extension js.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, Toolstrip) {
  FilePath extension_test_data_dir = test_data_dir_.AppendASCII("good").
      AppendASCII("Extensions").AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj").
      AppendASCII("1.0.0.0");
  ASSERT_TRUE(LoadExtension(extension_test_data_dir));

  // At this point, there should be two ExtensionHosts loaded because this
  // extension has two toolstrips. Find the one that is hosting toolstrip1.html.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/toolstrip1.html", 2);

  // Tell it to run some JavaScript that tests that basic extension code works.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testTabsAPI()", &result);
  EXPECT_TRUE(result);

#if defined(OS_WIN)
  // Test for compact language detection API. First navigate to a (static) html
  // file with a French sentence. Then, run the test API in toolstrip1.html to
  // actually call the language detection API through the existing extension,
  // and verify that the language returned is indeed French.
  FilePath language_url = extension_test_data_dir.AppendASCII(
      "french_sentence.html");
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(language_url.ToWStringHack()));

  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testTabsLanguageAPI()", &result);
  EXPECT_TRUE(result);
#endif
}

// Tests that the ExtensionShelf initializes properly, notices that
// an extension loaded and has a view available, and then sets that up
// properly.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, Shelf) {
  // When initialized, there are no extension views and the preferred height
  // should be zero.
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  ExtensionShelf* shelf = browser_view->extension_shelf();
  ASSERT_TRUE(shelf);
  EXPECT_EQ(shelf->GetChildViewCount(), 0);
  EXPECT_EQ(shelf->GetPreferredSize().height(), 0);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // There should now be two extension views and preferred height of the view
  // should be non-zero.
  EXPECT_EQ(shelf->GetChildViewCount(), 2);
  EXPECT_NE(shelf->GetPreferredSize().height(), 0);
}

// Tests that installing and uninstalling extensions don't crash with an
// incognito window open.
// This test is disabled. see bug 16106.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, Incognito) {
  // Open an incognito window to the extensions management page.  We just
  // want to make sure that we don't crash while playing with extensions when
  // this guy is around.
  Browser::OpenURLOffTheRecord(browser()->profile(),
                               GURL(chrome::kChromeUIExtensionsURL));

  ASSERT_TRUE(InstallExtension(test_data_dir_.AppendASCII("good.crx")));
  UninstallExtension("ldnnhddmnhbkjipkidpdiheffobcpfmf");
}

// Tests that we can load extension pages into the tab area and they can call
// extension APIs.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, TabContents) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));

  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"testTabsAPI()", &result);
  EXPECT_TRUE(result);
}

// Tests that we can load page actions in the Omnibox.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageAction) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("samples")
                    .AppendASCII("subscribe_page_action")));

  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));

  // Navigate to the feed page.
  FilePath test_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  FilePath feed = test_dir.AppendASCII("feeds")
                          .AppendASCII("feed.html");

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(feed));

  // We should now have one page action ready to go in the LocationBar.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  FilePath no_feed = test_dir.AppendASCII("feeds")
                             .AppendASCII("nofeed.html");

  // Make sure the page action goes away.
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(no_feed));
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));
}

GURL GetFeedUrl(const std::string& feed_page) {
  FilePath test_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_dir);

  FilePath subscribe;
  subscribe = test_dir.AppendASCII("extensions")
                      .AppendASCII("samples")
                      .AppendASCII("subscribe_page_action")
                      .AppendASCII("subscribe.html");
  subscribe = subscribe.StripTrailingSeparators();

  FilePath feed_dir = test_dir.AppendASCII("feeds")
                              .AppendASCII(feed_page.c_str());

  return GURL(net::FilePathToFileURL(subscribe).spec() +
              std::string("?") +
              net::FilePathToFileURL(feed_dir).spec());
}

static const wchar_t* jscript_feed_title =
    L"window.domAutomationController.send("
    L"  document.getElementById('title') ? "
    L"    document.getElementById('title').textContent : "
    L"    \"element 'title' not found\""
    L");";
static const wchar_t* jscript_anchor =
    L"window.domAutomationController.send("
    L"  document.getElementById('anchor_0') ? "
    L"    document.getElementById('anchor_0').textContent : "
    L"    \"element 'anchor_0' not found\""
    L");";
static const wchar_t* jscript_desc =
    L"window.domAutomationController.send("
    L"  document.getElementById('desc_0') ? "
    L"    document.getElementById('desc_0').textContent : "
    L"    \"element 'desc_0' not found\""
    L");";
static const wchar_t* jscript_error =
    L"window.domAutomationController.send("
    L"  document.getElementById('error') ? "
    L"    document.getElementById('error').textContent : "
    L"    \"No error\""
    L");";

void GetParsedFeedData(Browser* browser, std::string* feed_title,
                       std::string* item_title, std::string* item_desc,
                       std::string* error) {
  ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(), L"",
      jscript_feed_title, feed_title);
  ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(), L"",
      jscript_anchor, item_title);
  ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(), L"",
      jscript_desc, item_desc);
  ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser->GetSelectedTabContents()->render_view_host(), L"",
      jscript_error, error);
}

// Tests that we can parse feeds.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeed) {
  std::string feed_title;
  std::string item_title;
  std::string item_desc;
  std::string error;

  ui_test_utils::NavigateToURL(browser(), GetFeedUrl("feed1.xml"));
  GetParsedFeedData(browser(), &feed_title, &item_title, &item_desc, &error);
  EXPECT_STREQ("Feed for 'MyFeedTitle'", feed_title.c_str());
  EXPECT_STREQ("Title 1", item_title.c_str());
  EXPECT_STREQ("Desc", item_desc.c_str());
  EXPECT_STREQ("No error", error.c_str());

  ui_test_utils::NavigateToURL(browser(), GetFeedUrl("feed2.xml"));
  GetParsedFeedData(browser(), &feed_title, &item_title, &item_desc, &error);
  EXPECT_STREQ("Feed for 'MyFeed2'", feed_title.c_str());
  EXPECT_STREQ("My item title1", item_title.c_str());
  EXPECT_STREQ("This is a summary.", item_desc.c_str());
  EXPECT_STREQ("No error", error.c_str());

  // Try a feed that doesn't exist.
  ui_test_utils::NavigateToURL(browser(), GetFeedUrl("feed_nonexistant.xml"));
  GetParsedFeedData(browser(), &feed_title, &item_title, &item_desc, &error);
  EXPECT_STREQ("Feed for 'Unknown feed name'", feed_title.c_str());
  EXPECT_STREQ("element 'anchor_0' not found", item_title.c_str());
  EXPECT_STREQ("element 'desc_0' not found", item_desc.c_str());
  EXPECT_STREQ("Not a valid feed", error.c_str());

  // Try an empty feed.
  ui_test_utils::NavigateToURL(browser(), GetFeedUrl("feed_invalid1.xml"));
  GetParsedFeedData(browser(), &feed_title, &item_title, &item_desc, &error);
  EXPECT_STREQ("Feed for 'Unknown feed name'", feed_title.c_str());
  EXPECT_STREQ("element 'anchor_0' not found", item_title.c_str());
  EXPECT_STREQ("element 'desc_0' not found", item_desc.c_str());
  EXPECT_STREQ("Not a valid feed", error.c_str());

  // Try a garbage feed.
  ui_test_utils::NavigateToURL(browser(), GetFeedUrl("feed_invalid2.xml"));
  GetParsedFeedData(browser(), &feed_title, &item_title, &item_desc, &error);
  EXPECT_STREQ("Feed for 'Unknown feed name'", feed_title.c_str());
  EXPECT_STREQ("element 'anchor_0' not found", item_title.c_str());
  EXPECT_STREQ("element 'desc_0' not found", item_desc.c_str());
  EXPECT_STREQ("Not a valid feed", error.c_str());
}

// Tests that message passing between extensions and tabs works.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MessagingExtensionTab) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));

  // Get the ExtensionHost that is hosting our background page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/background.html", 1);

  // Load the tab that will communicate with our background page.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://bjafgdebaacbbbecmhlhpofkepfkgcpa/page.html"));

  // First test that tab->extension messaging works.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"testPostMessageFromTab()", &result);
  EXPECT_TRUE(result);

  // Now test extension->tab messaging, with disconnect events.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnect()", &result);
  EXPECT_TRUE(result);

  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testPostMessage()", &result);
  EXPECT_TRUE(result);

  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnectOnClose()", &result);
  EXPECT_TRUE(result);
}

// TODO(mpcomplete): reenable this when content script messaging is fixed:
// http://code.google.com/p/chromium/issues/detail?id=16228.
#if 0
// Tests that message passing between extensions and content scripts works.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MessagingContentScript) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));

  UserScriptMaster* master = browser()->profile()->GetUserScriptMaster();
  if (!master->ScriptsReady()) {
    // Wait for UserScriptMaster to finish its scan.
    NotificationRegistrar registrar;
    registrar.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                  NotificationService::AllSources());
    ui_test_utils::RunMessageLoop();
  }
  ASSERT_TRUE(master->ScriptsReady());

  // Get the ExtensionHost that is hosting our background page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/background.html", 1);

  // Load the tab whose content script will communicate with our background
  // page.
  FilePath test_file;
  PathService::Get(chrome::DIR_TEST_DATA, &test_file);
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("test_file.html");
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  // First test that tab->extension messaging works.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"testPostMessageFromTab()", &result);
  EXPECT_TRUE(result);

  // Now test extension->tab messaging, with disconnect events.
  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnect()", &result);
  EXPECT_TRUE(result);

  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testPostMessage()", &result);
  EXPECT_TRUE(result);

  result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testDisconnectOnClose()", &result);
  EXPECT_TRUE(result);
}
#endif
