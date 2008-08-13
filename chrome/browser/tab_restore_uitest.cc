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
    path_prefix += file_util::kPathSeparator;
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
    ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(tab_count,
                                                       &restored_tab_count,
                                                       5000));
    ASSERT_EQ(tab_count + 1, restored_tab_count);

    // Wait for the restored tab to finish loading.
    scoped_ptr<TabProxy> restored_tab_proxy(
        browser_proxy->GetTab(restored_tab_count - 1));
    ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(kWaitForActionMsec));
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
                                                     5000));
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
  }

  // Navigate to url1 then url2.
  scoped_ptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  tab_proxy->NavigateToURL(url1_);
  tab_proxy->NavigateToURL(url2_);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(SW_HIDE));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2,
                                                       kWaitForActionMaxMsec));

  // Close the first browser.
  EXPECT_TRUE(tab_proxy->Close(true));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(1,
                                                       kWaitForActionMaxMsec));

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
  TestServer server(kDocRoot);
  GURL http_url1(server.TestServerPageW(L"files/title1.html"));
  GURL http_url2(server.TestServerPageW(L"files/title2.html"));

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));

  // Add a tab
  browser_proxy->AppendTab(http_url1);
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(initial_tab_count,
                                                     &new_tab_count,
                                                     5000));
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
  TestServer server(kDocRoot);
  GURL http_url1(server.TestServerPageW(L"files/title1.html"));
  GURL http_url2(server.TestServerPageW(L"files/title2.html"));

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));

  // Add a tab
  browser_proxy->AppendTab(http_url1);
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->WaitForTabCountToChange(initial_tab_count,
                                                     &new_tab_count,
                                                     5000));
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
