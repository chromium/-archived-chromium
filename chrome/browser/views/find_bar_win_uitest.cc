// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

class FindInPageControllerTest : public UITest {
 public:
  FindInPageControllerTest() {
    show_window_ = true;
  }
};

const std::wstring kSimplePage = L"404_is_enough_for_us.html";

// The find window should not change its location just because we open and close
// a new tab.
TEST_F(FindInPageControllerTest, FindMovesOnTabClose_Issue1343052) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data", NULL);
  ASSERT_TRUE(NULL != server.get());

  GURL url = server->TestServerPageW(kSimplePage);
  scoped_refptr<TabProxy> tabA(GetActiveTab());
  ASSERT_TRUE(tabA.get());
  ASSERT_TRUE(tabA->NavigateToURL(url));
  WaitUntilTabCount(1);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get() != NULL);

  // Toggle the bookmark bar state.
  browser->ApplyAccelerator(IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(WaitForBookmarkBarVisibilityChange(browser.get(), true));

  // Open the Find window and wait for it to animate.
  EXPECT_TRUE(browser->OpenFindInPage());
  EXPECT_TRUE(WaitForFindWindowVisibilityChange(browser.get(), true));

  // Find its location.
  int x = -1, y = -1;
  EXPECT_TRUE(browser->GetFindWindowLocation(&x, &y));

  // Open another tab (tab B).
  EXPECT_TRUE(browser->AppendTab(url));
  scoped_refptr<TabProxy> tabB(GetActiveTab());
  ASSERT_TRUE(tabB.get());

  // Close tab B.
  EXPECT_TRUE(tabB->Close(true));

  // See if the Find window has moved.
  int new_x = -1, new_y = -1;
  EXPECT_TRUE(browser->GetFindWindowLocation(&new_x, &new_y));

  EXPECT_EQ(x, new_x);
  EXPECT_EQ(y, new_y);

  // Now reset the bookmark bar state and try the same again.
  browser->ApplyAccelerator(IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(WaitForBookmarkBarVisibilityChange(browser.get(), false));

  // Bookmark bar has moved, reset our coordinates.
  EXPECT_TRUE(browser->GetFindWindowLocation(&x, &y));

  // Open another tab (tab C).
  EXPECT_TRUE(browser->AppendTab(url));
  scoped_refptr<TabProxy> tabC(GetActiveTab());
  ASSERT_TRUE(tabC.get());

  // Close it.
  EXPECT_TRUE(tabC->Close(true));

  // See if the Find window has moved.
  EXPECT_TRUE(browser->GetFindWindowLocation(&new_x, &new_y));

  EXPECT_EQ(x, new_x);
  EXPECT_EQ(y, new_y);
}
