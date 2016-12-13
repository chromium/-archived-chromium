// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

// http://code.google.com/p/chromium/issues/detail?id=14774
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_BasicRestoreFromClosedWindow DISABLED_BasicRestoreFromClosedWindow
#else
#define MAYBE_BasicRestoreFromClosedWindow BasicRestoreFromClosedWindow
#endif

class TabRestoreUITest : public UITest {
 public:
  TabRestoreUITest() : UITest() {
    FilePath path_prefix(test_data_directory_);
    path_prefix = path_prefix.AppendASCII("session_history");
    url1_ = net::FilePathToFileURL(path_prefix.AppendASCII("bot1.html"));
    url2_ = net::FilePathToFileURL(path_prefix.AppendASCII("bot2.html"));
  }

 protected:
  // Uses the undo-close-tab accelerator to undo a close-tab or close-window
  // operation. The newly restored tab is expected to appear in the
  // window at index |expected_window_index|, at the |expected_tabstrip_index|,
  // and to be active. If |expected_window_index| is equal to the number of
  // current windows, the restored tab is expected to be created in a new
  // window (since the index is 0-based).
  void RestoreTab(int expected_window_index,
                  int expected_tabstrip_index) {
    int tab_count = 0;
    int window_count = 0;

    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_GT(window_count, 0);

    bool expect_new_window = (expected_window_index == window_count);
    scoped_refptr<BrowserProxy> browser_proxy;
    if (expect_new_window) {
      browser_proxy = automation()->GetBrowserWindow(0);
    } else {
      ASSERT_GT(window_count, expected_window_index);
      browser_proxy = automation()->GetBrowserWindow(expected_window_index);
    }
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
    ASSERT_GT(tab_count, 0);

    // Restore the tab.
    ASSERT_TRUE(browser_proxy->RunCommand(IDC_RESTORE_TAB));

    if (expect_new_window) {
      int new_window_count = 0;
      ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
      EXPECT_EQ(++window_count, new_window_count);
      browser_proxy = automation()->GetBrowserWindow(expected_window_index);
    } else {
      int new_tab_count = 0;
      ASSERT_TRUE(browser_proxy->GetTabCount(&new_tab_count));
      EXPECT_EQ(++tab_count, new_tab_count);
    }

    // Get a handle to the restored tab.
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
    ASSERT_GT(tab_count, expected_tabstrip_index);
    scoped_refptr<TabProxy> restored_tab_proxy(
        browser_proxy->GetTab(expected_tabstrip_index));
    ASSERT_TRUE(restored_tab_proxy.get());
    // Wait for the restored tab to finish loading.
    ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(
        action_max_timeout_ms()));

    // Ensure that the tab and window are active.
    CheckActiveWindow(browser_proxy.get());
    EXPECT_EQ(expected_tabstrip_index,
        GetActiveTabIndex(expected_window_index));
  }

  // Adds tabs to the given browser, all navigated to url1_. Returns
  // the final number of tabs.
  int AddSomeTabs(BrowserProxy* browser, int how_many) {
    int starting_tab_count = -1;
    // Use EXPECT instead of ASSERT throughout to avoid trying to return void.
    EXPECT_TRUE(browser->GetTabCount(&starting_tab_count));

    for (int i = 0; i < how_many; ++i) {
      browser->AppendTab(url1_);
      int current_tab_count;
      EXPECT_TRUE(browser->GetTabCount(&current_tab_count));
      EXPECT_EQ(starting_tab_count + i + 1, current_tab_count);
    }
    int tab_count;
    EXPECT_TRUE(browser->GetTabCount(&tab_count));
    EXPECT_EQ(starting_tab_count + how_many, tab_count);
    return tab_count;
  }

  // Ensure that the given browser occupies the currently active window.
  void CheckActiveWindow(const BrowserProxy* browser) {
    // This entire check is disabled because even the IsActive() call
    // sporadically fails to complete successfully. See http://crbug.com/10916.
    // TODO(pamg): Investigate and re-enable. Also find a way to have the
    // calling location reported in the gtest error, by inlining this again if
    // nothing else.
    return;

    bool is_active = false;
    scoped_refptr<WindowProxy> window_proxy(browser->GetWindow());
    ASSERT_TRUE(window_proxy->IsActive(&is_active));
    // The check for is_active may fail if other apps are active while running
    // the tests, because Chromium won't be the foremost application at all.
    // So we'll have it log an error, but not report one through gtest, to
    // keep the test result deterministic and the buildbots happy.
    if (!is_active)
      LOG(ERROR) << "WARNING: is_active was false, expected true. (This may "
                    "be simply because Chromium isn't the front application.)";
  }

  GURL url1_;
  GURL url2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabRestoreUITest);
};

// Close the end tab in the current window, then restore it. The tab should be
// in its original position, and active.
TEST_F(TabRestoreUITest, Basic) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 1);

  int closed_tab_index = tab_count - 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  new_tab->NavigateToURL(url1_);
  // Close the tab.
  new_tab->Close(true);
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count, tab_count);

  RestoreTab(0, closed_tab_index);

  // And make sure everything looks right.
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 1, tab_count);
  EXPECT_EQ(closed_tab_index, GetActiveTabIndex());
  EXPECT_EQ(url1_, GetActiveTabURL());
}

// Close a tab not at the end of the current window, then restore it. The tab
// should be in its original position, and active.
TEST_F(TabRestoreUITest, MiddleTab) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  new_tab->NavigateToURL(url1_);
  // Close the tab.
  new_tab->Close(true);
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  RestoreTab(0, closed_tab_index);

  // And make sure everything looks right.
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 3, tab_count);
  EXPECT_EQ(closed_tab_index, GetActiveTabIndex());
  EXPECT_EQ(url1_, GetActiveTabURL());
}

// Close a tab, switch windows, then restore the tab. The tab should be in its
// original window and position, and active.
// Disabled because flacky. See http://crbug.com/14132
TEST_F(TabRestoreUITest, DISABLED_RestoreToDifferentWindow) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));

  // This call is virtually guaranteed to pass, assuming that Chromium is the
  // active application, which will establish a baseline for later calls to
  // CheckActiveWindow(). See comments in that function.
  CheckActiveWindow(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  new_tab->NavigateToURL(url1_);
  // Close the tab.
  new_tab->Close(true);
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);

  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Restore tab into original browser.
  RestoreTab(0, closed_tab_index);

  // And make sure everything looks right.
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 3, tab_count);
  EXPECT_EQ(closed_tab_index, GetActiveTabIndex(0));
  EXPECT_EQ(url1_, GetActiveTabURL(0));
}

// Close a tab, open a new window, close the first window, then restore the
// tab. It should be in a new window.
TEST_F(TabRestoreUITest, MAYBE_BasicRestoreFromClosedWindow) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  CheckActiveWindow(browser_proxy.get());

  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Close tabs until we only have one open.
  while (tab_count > 1) {
    scoped_refptr<TabProxy> tab_to_close(browser_proxy->GetTab(0));
    ASSERT_TRUE(tab_to_close.get());
    tab_to_close->Close(true);
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  }

  // Navigate to url1 then url2.
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  tab_proxy->NavigateToURL(url1_);
  tab_proxy->NavigateToURL(url2_);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);
  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Close the final tab in the first browser.
  EXPECT_TRUE(tab_proxy->Close(true));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(
      1, action_max_timeout_ms()));

  // Tab and browser are no longer valid.
  tab_proxy = NULL;
  browser_proxy = NULL;

  RestoreTab(1, 0);

  // Tab should be in a new window.
  browser_proxy = automation()->GetBrowserWindow(1);
  CheckActiveWindow(browser_proxy.get());
  tab_proxy = browser_proxy->GetActiveTab();
  ASSERT_TRUE(tab_proxy.get());
  // And make sure the URLs matches.
  EXPECT_EQ(url2_, GetActiveTabURL(1));
  EXPECT_TRUE(tab_proxy->GoBack());
  EXPECT_EQ(url1_, GetActiveTabURL(1));
}

// Restore a tab then make sure it doesn't restore again.
// Disabled because the command updater doesn't know the proper state of
// the tab restore command. http://crbug.com/14428.
TEST_F(TabRestoreUITest, DISABLED_DontLoadRestoredTab) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  CheckActiveWindow(browser_proxy.get());

  // Add two tabs
  int starting_tab_count = 0;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  AddSomeTabs(browser_proxy.get(), 2);
  int current_tab_count = 0;
  ASSERT_TRUE(browser_proxy->GetTabCount(&current_tab_count));
  ASSERT_EQ(current_tab_count, starting_tab_count + 2);

  // Close one of them.
  scoped_refptr<TabProxy> tab_to_close(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_to_close.get());
  tab_to_close->Close(true);
  ASSERT_TRUE(browser_proxy->GetTabCount(&current_tab_count));
  ASSERT_EQ(current_tab_count, starting_tab_count + 1);

  // Restore it.
  RestoreTab(0, 0);
  ASSERT_TRUE(browser_proxy->GetTabCount(&current_tab_count));
  ASSERT_EQ(current_tab_count, starting_tab_count + 2);

  // Make sure that there's nothing else to restore.
  // TODO(pinkerton): This currently fails because the command_updater in the
  // always says yes. See bug above.
  bool is_timeout = false;
  bool enabled =
    browser_proxy->IsPageMenuCommandEnabledWithTimeout(IDC_RESTORE_TAB,
        action_max_timeout_ms(), &is_timeout);
  if (!is_timeout)
    ASSERT_FALSE(enabled);
}

// Open a window with multiple tabs, close a tab, then close the window.
// Restore both and make sure the tab goes back into the window.
// Disabled because flakey. See http://crbug.com/14132
TEST_F(TabRestoreUITest, DISABLED_RestoreWindowAndTab) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  CheckActiveWindow(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  new_tab->NavigateToURL(url1_);
  // Close the tab.
  new_tab->Close(true);
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);
  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Close the first browser.
  bool application_closing;
  EXPECT_TRUE(CloseBrowser(browser_proxy.get(), &application_closing));
  EXPECT_FALSE(application_closing);
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);

  // Browser is no longer valid.
  browser_proxy = NULL;

  // Restore the first window. The expected_tabstrip_index (second argument)
  // indicates the expected active tab.
  RestoreTab(1, starting_tab_count + 1);
  browser_proxy = automation()->GetBrowserWindow(1);
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  // Restore the closed tab.
  RestoreTab(1, closed_tab_index);
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 3, tab_count);
  EXPECT_EQ(url1_, GetActiveTabURL(1));
}

// Open a window with two tabs, close both (closing the window), then restore
// both. Make sure both restored tabs are in the same window.
TEST_F(TabRestoreUITest, RestoreIntoSameWindow) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  CheckActiveWindow(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 2);

  // Navigate the rightmost one to url2_ for easier identification.
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(tab_proxy.get());
  tab_proxy->NavigateToURL(url2_);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);
  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Close all but one tab in the first browser, left to right.
  while (tab_count > 1) {
    scoped_refptr<TabProxy> tab_to_close(browser_proxy->GetTab(0));
    ASSERT_TRUE(tab_to_close.get());
    tab_to_close->Close(true);
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  }

  // Close the last tab, closing the browser.
  tab_proxy = browser_proxy->GetTab(0);
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_TRUE(tab_proxy->Close(true));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(
      1, action_max_timeout_ms()));
  browser_proxy = NULL;
  tab_proxy = NULL;

  // Restore the last-closed tab into a new window.
  RestoreTab(1, 0);
  browser_proxy = automation()->GetBrowserWindow(1);
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(1, tab_count);
  EXPECT_EQ(url2_, GetActiveTabURL(1));

  // Restore the next-to-last-closed tab into the same window.
  RestoreTab(1, 0);
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(2, tab_count);
  EXPECT_EQ(url1_, GetActiveTabURL(1));
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

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Add a tab
  browser_proxy->AppendTab(http_url1);
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&new_tab_count));
  EXPECT_EQ(++tab_count, new_tab_count);
  scoped_refptr<TabProxy> tab(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(tab.get());

  // Navigate to another same-site URL.
  tab->NavigateToURL(http_url2);

  // Close the tab.
  tab->Close(true);
  tab = NULL;

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance used by the restored tab
  // will already exist when the restore happens.
  browser_proxy->AppendTab(http_url2);

  // Restore the closed tab.
  RestoreTab(0, tab_count - 1);
  tab = browser_proxy->GetActiveTab();
  ASSERT_TRUE(tab.get());

  // And make sure the URLs match.
  EXPECT_EQ(http_url2, GetActiveTabURL());
  EXPECT_TRUE(tab->GoBack());
  EXPECT_EQ(http_url1, GetActiveTabURL());
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

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Add a tab
  browser_proxy->AppendTab(http_url1);
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&new_tab_count));
  EXPECT_EQ(++tab_count, new_tab_count);
  scoped_refptr<TabProxy> tab(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(tab.get());

  // Navigate to more URLs, then a cross-site URL.
  tab->NavigateToURL(http_url2);
  tab->NavigateToURL(http_url1);
  tab->NavigateToURL(url1_);

  // Close the tab.
  tab->Close(true);
  tab = NULL;

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance will already exist when
  // the user clicks Back in the restored tab.
  browser_proxy->AppendTab(http_url2);

  // Restore the closed tab.
  RestoreTab(0, tab_count - 1);
  tab = browser_proxy->GetActiveTab();
  ASSERT_TRUE(tab.get());

  // And make sure the URLs match.
  EXPECT_EQ(url1_, GetActiveTabURL());
  EXPECT_TRUE(tab->GoBack());
  EXPECT_EQ(http_url1, GetActiveTabURL());

  // Navigating to a new URL should clear the forward list, because the max
  // page ID of the renderer should have been updated when we restored the tab.
  tab->NavigateToURL(http_url2);
  EXPECT_FALSE(tab->GoForward());
  EXPECT_EQ(http_url2, GetActiveTabURL());
}

TEST_F(TabRestoreUITest, RestoreWindow) {
  // Create a new window.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(false));
  int new_window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
  EXPECT_EQ(++window_count, new_window_count);

  // Create two more tabs, one with url1, the other url2.
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));
  browser_proxy->AppendTab(url1_);
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(initial_tab_count + 1,
                                                     action_max_timeout_ms()));
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(initial_tab_count));
  ASSERT_TRUE(new_tab.get());
  new_tab->NavigateToURL(url1_);
  browser_proxy->AppendTab(url2_);
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(initial_tab_count + 2,
                                                     action_max_timeout_ms()));
  new_tab = browser_proxy->GetTab(initial_tab_count + 1);
  ASSERT_TRUE(new_tab.get());
  new_tab->NavigateToURL(url2_);

  // Close the window.
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_CLOSE_WINDOW));
  browser_proxy = NULL;
  new_tab = NULL;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
  EXPECT_EQ(window_count - 1, new_window_count);

  // Restore the window.
  browser_proxy = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_RESTORE_TAB));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
  EXPECT_EQ(window_count, new_window_count);

  browser_proxy = automation()->GetBrowserWindow(1);
  int tab_count;
  EXPECT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(initial_tab_count + 2, tab_count);

  scoped_refptr<TabProxy> restored_tab_proxy(
        browser_proxy->GetTab(initial_tab_count));
  ASSERT_TRUE(restored_tab_proxy.get());
  ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));
  GURL url;
  ASSERT_TRUE(restored_tab_proxy->GetCurrentURL(&url));
  EXPECT_TRUE(url == url1_);

  restored_tab_proxy = browser_proxy->GetTab(initial_tab_count + 1);
  ASSERT_TRUE(restored_tab_proxy.get());
  ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(action_timeout_ms()));
  ASSERT_TRUE(restored_tab_proxy->GetCurrentURL(&url));
  EXPECT_TRUE(url == url2_);
}
