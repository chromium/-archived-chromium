// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automated_ui_tests/automated_ui_test_base.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

namespace {

bool WaitForURLDisplayedForTab(BrowserProxy* browser, int tab_index,
                               GURL url) {
  int i = 0;
  while (i < 10) {
    scoped_refptr<TabProxy> tab(browser->GetTab(tab_index));
    GURL tab_url;
    if (tab->GetCurrentURL(&tab_url))
      if (tab_url == url)
        return true;
    i++;
    PlatformThread::Sleep(automation::kSleepTime);
  }
  return false;
}

}

TEST_F(AutomatedUITestBase, DragOut) {
  int tab_count;
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  ASSERT_FALSE(DragTabOut());
  NewTab();
  Navigate(GURL(L"about:"));
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);
  GURL chrome_downloads_url(L"chrome://downloads/");
  Navigate(chrome_downloads_url);
  ASSERT_TRUE(WaitForURLDisplayedForTab(active_browser(), 2,
                                        chrome_downloads_url));
  ASSERT_TRUE(DragTabOut());
  int window_count;
  automation()->GetBrowserWindowCount(&window_count);
  ASSERT_EQ(2, window_count);
}

TEST_F(AutomatedUITestBase, DragLeftRight) {
  int tab_count;
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  ASSERT_FALSE(DragActiveTab(false));

  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  GURL about_url(L"about:");
  Navigate(about_url);

  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);
  GURL chrome_downloads_url(L"chrome://downloads/");
  Navigate(chrome_downloads_url);
  ASSERT_TRUE(WaitForURLDisplayedForTab(active_browser(), 2,
                                        chrome_downloads_url));

  // Drag the active tab to left. Now download tab should be the middle tab.
  ASSERT_TRUE(DragActiveTab(false));
  ASSERT_TRUE(WaitForURLDisplayedForTab(active_browser(), 1,
                                        chrome_downloads_url));

  // Drag the active tab to left. Now download tab should be the leftmost
  // tab.
  ASSERT_TRUE(DragActiveTab(false));
  ASSERT_TRUE(WaitForURLDisplayedForTab(active_browser(), 0,
                                        chrome_downloads_url));

  // Drag the active tab to left. It should fail since the active tab is
  // already the leftmost tab.
  ASSERT_FALSE(DragActiveTab(false));

  // Drag the active tab to right. Now download tab should be the middle tab.
  ASSERT_TRUE(DragActiveTab(true));
  ASSERT_TRUE(WaitForURLDisplayedForTab(active_browser(), 1,
                                        chrome_downloads_url));

  // Drag the active tab to right. Now download tab should be the rightmost
  // tab.
  ASSERT_TRUE(DragActiveTab(true));
  ASSERT_TRUE(WaitForURLDisplayedForTab(active_browser(), 2,
                                        chrome_downloads_url));

  // Drag the active tab to right. It should fail since the active tab is
  // already the rightmost tab.
  ASSERT_FALSE(DragActiveTab(true));
}
