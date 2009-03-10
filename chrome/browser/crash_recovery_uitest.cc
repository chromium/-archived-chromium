// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef UITest CrashRecoveryUITest;

TEST_F(CrashRecoveryUITest, Reload) {
  // Test that reload works after a crash.

  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  // The title of the active tab should change each time this URL is loaded.
  GURL url(
      "data:text/html,<script>document.title=new Date().valueOf()</script>");

  NavigateToURL(url);

  std::wstring title1 = GetActiveTabTitle();

  scoped_ptr<TabProxy> tab(GetActiveTab());

  // Cause the renderer to crash.
  expected_crashes_ = 1;
  tab->NavigateToURLAsync(GURL("about:crash"));

  Sleep(1000);  // Wait for the browser to notice the renderer crash.

  tab->Reload();

  std::wstring title2 = GetActiveTabTitle();
  EXPECT_NE(title1, title2);
}

// Tests that loading a crashed page in a new tab correctly updates the title.
// There was an earlier bug (1270510) in process-per-site in which the max page
// ID of the RenderProcessHost was stale, so the NavigationEntry in the new tab
// was not committed.  This prevents regression of that bug.
TEST_F(CrashRecoveryUITest, LoadInNewTab) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  // The title of the active tab should change each time this URL is loaded.
  std::wstring test_file = test_data_directory_;
  file_util::AppendToPath(&test_file, L"title2.html");
  GURL url(net::FilePathToFileURL(test_file));

  NavigateToURL(url);

  const std::wstring title(L"Title Of Awesomeness");
  EXPECT_EQ(title, GetActiveTabTitle());

  scoped_ptr<TabProxy> tab(GetActiveTab());

  // Cause the renderer to crash.
  expected_crashes_ = 1;
  tab->NavigateToURLAsync(GURL("about:crash"));

  Sleep(1000);  // Wait for the browser to notice the renderer crash.

  scoped_ptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy->AppendTab(url));

  // Ensure the title of the new tab is updated, indicating that the navigation
  // entry was properly committed.
  EXPECT_EQ(title, GetActiveTabTitle());
}
