// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automated_ui_tests/automated_ui_test_base.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"

TEST_F(AutomatedUITestBase, NewTab) {
  ASSERT_TRUE(automation()->WaitForInitialLoads());
  set_active_browser(automation()->GetBrowserWindow(0));
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
