// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/no_windows2000_unittest.h"
#include "base/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

// This test does not work on win2k. See http://b/1070036
class InspectorControllerTest : public NoWindows2000Test<UITest> {
 protected:
  TabProxy* GetActiveTabProxy() {
    scoped_ptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(window_proxy.get());

    int active_tab_index = 0;
    EXPECT_TRUE(window_proxy->GetActiveTabIndex(&active_tab_index));
    return window_proxy->GetTab(active_tab_index);
  }

  void NavigateTab(TabProxy* tab_proxy, const GURL& url) {
    ASSERT_TRUE(tab_proxy->NavigateToURL(url));
  }
};

// This test also does not work in single process. See http://b/1214920
TEST_F(InspectorControllerTest, InspectElement) {
  if (IsTestCaseDisabled())
    return;

  if (CommandLine().HasSwitch(switches::kSingleProcess))
    return;

  TestServer server(L"chrome/test/data");
  ::scoped_ptr<TabProxy> tab(GetActiveTabProxy());
  // We don't track resources until we've opened the inspector.
  NavigateTab(tab.get(), server.TestServerPageW(L"files/inspector/test1.html"));
  tab->InspectElement(0, 0);
  NavigateTab(tab.get(), server.TestServerPageW(L"files/inspector/test1.html"));
  EXPECT_EQ(1, tab->InspectElement(0, 0));
  NavigateTab(tab.get(), server.TestServerPageW(L"files/inspector/test2.html"));
  EXPECT_EQ(2, tab->InspectElement(0, 0));
}
