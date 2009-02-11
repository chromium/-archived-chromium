// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/win_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

class TabRestoreUITest : public UITest {
 public:
  TabRestoreUITest() : UITest() {
    std::wstring path_prefix = test_data_directory_;
    file_util::AppendToPath(&path_prefix, L"session_history");
    path_prefix += FilePath::kSeparators[0];
    url1_ = net::FilePathToFileURL(path_prefix + L"bot1.html");
    url2_ = net::FilePathToFileURL(path_prefix + L"bot2.html");
  }

 protected:
  void RestoreTab() {
    int tab_count;

    // Reset browser_proxy to new window.
    scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
    ASSERT_GT(tab_count, 0);

    // Restore the tab.
    ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_RESTORE_TAB));

    // Get a handle to the restored tab.
    int restored_tab_count;
    ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(
        tab_count, &restored_tab_count, action_max_timeout_ms()));
    ASSERT_EQ(tab_count + 1, restored_tab_count);

    // Wait for the restored tab to finish loading.
    scoped_ptr<TabProxy> restored_tab_proxy(
        browser_proxy->GetTab(restored_tab_count - 1));
    ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(
        action_max_timeout_ms()));
  }

  GURL url1_;
  GURL url2_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TabRestoreUITest);
};

TEST_F(TabRestoreUITest, Basic) {
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));

  // Add a tab
  browser_proxy->AppendTab(url1_);
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(initial_tab_count,
                                                     &new_tab_count,
                                                     action_max_timeout_ms()));
  scoped_ptr<TabProxy> new_tab(browser_proxy->GetTab(new_tab_count - 1));
  // Make sure we're at url.
  new_tab->NavigateToURL(url1_);
  // Close the tab.
  new_tab->Close(true);
  new_tab.reset();

  RestoreTab();

  // And make sure the URL matches.
  ASSERT_EQ(url1_, GetActiveTabURL());
}

TEST_F(TabRestoreUITest, RestoreToDifferentWindow) {
  // This test is disabled on win2k. See bug 1215881.
  if (win_util::GetWinVersion() == win_util::WINVERSION_2000)
    return;

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Close tabs until we only have one open.
  while (tab_count > 1) {
    scoped_ptr<TabProxy> tab_to_close(browser_proxy->GetTab(0));
    tab_to_close->Close(true);
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  }

  // Navigate to url1 then url2.
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  tab_proxy->NavigateToURL(url1_);
  tab_proxy->NavigateToURL(url2_);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(SW_HIDE));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(
      2, action_max_timeout_ms()));

  // Close the first browser.
  EXPECT_TRUE(tab_proxy->Close(true));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(
      1, action_max_timeout_ms()));

  // Tab and browser are no longer valid.
  tab_proxy.reset();
  browser_proxy.reset();

  RestoreTab();

  browser_proxy.reset(automation()->GetBrowserWindow(0));
  tab_proxy.reset(browser_proxy->GetActiveTab());
  // And make sure the URLs matches.
  ASSERT_EQ(url2_, GetActiveTabURL());
  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_EQ(url1_, GetActiveTabURL());
}

// Tests that a duplicate history entry is not created when we restore a page
// to an existing SiteInstance.  (Bug 1230446)
TEST_F(TabRestoreUITest, RestoreWithExistingSiteInstance) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  GURL http_url1(server->TestServerPageW(L"files/title1.html"));
  GURL http_url2(server->TestServerPageW(L"files/title2.html"));

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));

  // Add a tab
  browser_proxy->AppendTab(http_url1);
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(initial_tab_count,
                                                     &new_tab_count,
                                                     action_max_timeout_ms()));
  scoped_ptr<TabProxy> tab(browser_proxy->GetTab(new_tab_count - 1));

  // Navigate to another same-site URL.
  tab->NavigateToURL(http_url2);

  // Close the tab.
  tab->Close(true);
  tab.reset();

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance used by the restored tab
  // will already exist when the restore happens.
  browser_proxy->AppendTab(http_url2);

  // Restore the closed tab.
  RestoreTab();
  tab.reset(browser_proxy->GetActiveTab());

  // And make sure the URLs match.
  ASSERT_EQ(http_url2, GetActiveTabURL());
  ASSERT_TRUE(tab->GoBack());
  ASSERT_EQ(http_url1, GetActiveTabURL());
}

// Tests that the SiteInstances used for entries in a restored tab's history
// are given appropriate max page IDs, even if the renderer for the entry
// already exists.  (Bug 1204135)
TEST_F(TabRestoreUITest, RestoreCrossSiteWithExistingSiteInstance) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  GURL http_url1(server->TestServerPageW(L"files/title1.html"));
  GURL http_url2(server->TestServerPageW(L"files/title2.html"));

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));

  // Add a tab
  browser_proxy->AppendTab(http_url1);
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(initial_tab_count,
                                                     &new_tab_count,
                                                     action_max_timeout_ms()));
  scoped_ptr<TabProxy> tab(browser_proxy->GetTab(new_tab_count - 1));

  // Navigate to more URLs, then a cross-site URL.
  tab->NavigateToURL(http_url2);
  tab->NavigateToURL(http_url1);
  tab->NavigateToURL(url1_);

  // Close the tab.
  tab->Close(true);
  tab.reset();

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance will already exist when
  // the user clicks Back in the restored tab.
  browser_proxy->AppendTab(http_url2);

  // Restore the closed tab.
  RestoreTab();
  tab.reset(browser_proxy->GetActiveTab());

  // And make sure the URLs match.
  ASSERT_EQ(url1_, GetActiveTabURL());
  ASSERT_TRUE(tab->GoBack());
  ASSERT_EQ(http_url1, GetActiveTabURL());

  // Navigating to a new URL should clear the forward list, because the max
  // page ID of the renderer should have been updated when we restored the tab.
  tab->NavigateToURL(http_url2);
  ASSERT_FALSE(tab->GoForward());
  ASSERT_EQ(http_url2, GetActiveTabURL());
}

TEST_F(TabRestoreUITest, RestoreWindow) {
  // Create a new window.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(SW_HIDE));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(
      ++window_count, action_max_timeout_ms()));

  // Create two more tabs, one with url1, the other url2.
  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));
  browser_proxy->AppendTab(url1_);
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(initial_tab_count + 1,
                                                     action_max_timeout_ms()));
  scoped_ptr<TabProxy> new_tab(browser_proxy->GetTab(initial_tab_count));
  new_tab->NavigateToURL(url1_);
  browser_proxy->AppendTab(url2_);
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(initial_tab_count + 2,
                                                     action_max_timeout_ms()));
  new_tab.reset(browser_proxy->GetTab(initial_tab_count + 1));
  new_tab->NavigateToURL(url2_);

  // Close the window.
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_CLOSE_WINDOW));
  browser_proxy.reset();
  new_tab.reset();
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(
      window_count - 1, action_max_timeout_ms()));

  // Restore the window.
  browser_proxy.reset(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy->ApplyAccelerator(IDC_RESTORE_TAB));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(
      window_count, action_max_timeout_ms()));

  browser_proxy.reset(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(initial_tab_count + 2,
                                                     action_max_timeout_ms()));

  scoped_ptr<TabProxy> restored_tab_proxy(
        browser_proxy->GetTab(initial_tab_count));
  ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));
  GURL url;
  ASSERT_TRUE(restored_tab_proxy->GetCurrentURL(&url));
  ASSERT_TRUE(url == url1_);

  restored_tab_proxy.reset(
        browser_proxy->GetTab(initial_tab_count + 1));
  ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));
  ASSERT_TRUE(restored_tab_proxy->GetCurrentURL(&url));
  ASSERT_TRUE(url == url2_);
}
