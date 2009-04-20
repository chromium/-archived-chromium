// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automated_ui_tests/automated_ui_test_base.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"

TEST_F(AutomatedUITestBase, NewTab) {
  int tab_count;
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);
}

TEST_F(AutomatedUITestBase, DuplicateTab) {
  int tab_count;
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  DuplicateTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  DuplicateTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);
}

TEST_F(AutomatedUITestBase, OpenCloseBrowserWindow) {
  int num_browser_windows;
  int tab_count;
  automation()->GetBrowserWindowCount(&num_browser_windows);
  ASSERT_EQ(1, num_browser_windows);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);

  BrowserProxy* previous_browser;
  ASSERT_TRUE(OpenAndActivateNewBrowserWindow(&previous_browser));
  scoped_ptr<BrowserProxy>browser_1(previous_browser);
  automation()->GetBrowserWindowCount(&num_browser_windows);
  ASSERT_EQ(2, num_browser_windows);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  NewTab();
  browser_1->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);

  ASSERT_TRUE(OpenAndActivateNewBrowserWindow(&previous_browser));
  scoped_ptr<BrowserProxy>browser_2(previous_browser);
  automation()->GetBrowserWindowCount(&num_browser_windows);
  ASSERT_EQ(3, num_browser_windows);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  NewTab();
  NewTab();
  browser_1->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  browser_2->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);

  bool application_closed;
  CloseBrowser(browser_1.get(), &application_closed);
  ASSERT_FALSE(application_closed);
  automation()->GetBrowserWindowCount(&num_browser_windows);
  ASSERT_EQ(2, num_browser_windows);
  CloseBrowser(browser_2.get(), &application_closed);
  ASSERT_FALSE(application_closed);
  automation()->GetBrowserWindowCount(&num_browser_windows);
  ASSERT_EQ(1, num_browser_windows);
}
