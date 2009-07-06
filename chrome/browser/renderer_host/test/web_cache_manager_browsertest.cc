// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebCacheManagerBrowserTest : public InProcessBrowserTest {
};

// Regression test for http://crbug.com/12362.  If a renderer crashes and the
// user navigates to another tab and back, the browser doesn't crash.
// TODO(jam): http://crbug.com/15288 disabled because it fails on the build bot.
IN_PROC_BROWSER_TEST_F(WebCacheManagerBrowserTest, DISABLED_CrashOnceOnly) {
  GURL url(ui_test_utils::GetTestUrl(L"google", L"google.html"));

  ui_test_utils::NavigateToURL(browser(), url);

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* tab = browser()->GetTabContentsAt(0);
  ASSERT_TRUE(tab != NULL);
  base::KillProcess(
      tab->process()->process().handle(), base::PROCESS_END_KILLED_BY_USER,
      true);

  browser()->SelectTabContentsAt(0, true);
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->SelectTabContentsAt(0, true);
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  // We would have crashed at the above line with the bug.

  browser()->SelectTabContentsAt(0, true);
  browser()->CloseTab();
  browser()->SelectTabContentsAt(0, true);
  browser()->CloseTab();
  browser()->SelectTabContentsAt(0, true);
  browser()->CloseTab();

  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(
      WebCacheManager::GetInstance()->active_renderers_.size(), 1U);
  EXPECT_EQ(
      WebCacheManager::GetInstance()->inactive_renderers_.size(), 0U);
  EXPECT_EQ(
      WebCacheManager::GetInstance()->stats_.size(), 1U);
}
