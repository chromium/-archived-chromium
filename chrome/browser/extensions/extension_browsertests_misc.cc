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
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"

// Tests that toolstrips initializes properly and can run basic extension js.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, Toolstrip) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // At this point, there should be two ExtensionHosts loaded because this
  // extension has two toolstrips. Find the one that is hosting toolstrip1.html.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = NULL;
  int num_hosts = 0;
  for (ExtensionProcessManager::const_iterator iter = manager->begin();
       iter != manager->end(); ++iter) {
    if ((*iter)->GetURL().path() == "/toolstrip1.html") {
      ASSERT_FALSE(host);
      host = *iter;
    }
    num_hosts++;
  }
  EXPECT_EQ(2, num_hosts);

  // Tell it to run some JavaScript that tests that basic extension code works.
  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testTabsAPI()", &result);
  EXPECT_TRUE(result);
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
