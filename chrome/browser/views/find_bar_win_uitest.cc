// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/views/find_bar_win.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

class FindInPageControllerTest : public UITest {
 public:
  FindInPageControllerTest() {
    show_window_ = true;
  }
};

const std::wstring kFramePage = L"files/find_in_page/frames.html";
const std::wstring kFrameData = L"files/find_in_page/framedata_general.html";
const std::wstring kUserSelectPage = L"files/find_in_page/user-select.html";
const std::wstring kCrashPage = L"files/find_in_page/crash_1341577.html";
const std::wstring kTooFewMatchesPage = L"files/find_in_page/bug_1155639.html";

// This test loads a page with frames and starts FindInPage requests
TEST_F(FindInPageControllerTest, FindInPageFrames) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to our frames page.
  GURL url = server->TestServerPageW(kFramePage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  // Try incremental search (mimicking user typing in).
  EXPECT_EQ(18, tab->FindInPage(L"g",       FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(11, tab->FindInPage(L"go",      FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(04, tab->FindInPage(L"goo",     FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(03, tab->FindInPage(L"goog",    FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(02, tab->FindInPage(L"googl",   FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(01, tab->FindInPage(L"google",  FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(00, tab->FindInPage(L"google!", FWD, IGNORE_CASE, false, NULL));

  // Negative test (no matches should be found).
  EXPECT_EQ(0, tab->FindInPage(L"Non-existing string", FWD, IGNORE_CASE,
                               false, NULL));

  // 'horse' only exists in the three right frames.
  EXPECT_EQ(3, tab->FindInPage(L"horse", FWD, IGNORE_CASE, false, NULL));

  // 'cat' only exists in the first frame.
  EXPECT_EQ(1, tab->FindInPage(L"cat", FWD, IGNORE_CASE, false, NULL));

  // Try searching again, should still come up with 1 match.
  EXPECT_EQ(1, tab->FindInPage(L"cat", FWD, IGNORE_CASE, false, NULL));

  // Try searching backwards, ignoring case, should still come up with 1 match.
  EXPECT_EQ(1, tab->FindInPage(L"CAT", BACK, IGNORE_CASE, false, NULL));

  // Try case sensitive, should NOT find it.
  EXPECT_EQ(0, tab->FindInPage(L"CAT", FWD, CASE_SENSITIVE, false, NULL));

  // Try again case sensitive, but this time with right case.
  EXPECT_EQ(1, tab->FindInPage(L"dog", FWD, CASE_SENSITIVE, false, NULL));

  // Try non-Latin characters ('Hreggvidur' with 'eth' for 'd' in left frame).
  EXPECT_EQ(1, tab->FindInPage(L"Hreggvi\u00F0ur", FWD, IGNORE_CASE,
                               false, NULL));
  EXPECT_EQ(1, tab->FindInPage(L"Hreggvi\u00F0ur", FWD, CASE_SENSITIVE,
                               false, NULL));
  EXPECT_EQ(0, tab->FindInPage(L"hreggvi\u00F0ur", FWD, CASE_SENSITIVE,
                               false, NULL));
}

// This test loads a single-frame page and makes sure the ordinal returned makes
// sense as we FindNext over all the items.
TEST_F(FindInPageControllerTest, FindInPageOrdinal) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to our frames page.
  GURL url = server->TestServerPageW(kFrameData);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  // Search for 'o', which should make the first item active and return
  // '1 in 3' (1st ordinal of a total of 3 matches).
  int ordinal = 0;
  EXPECT_EQ(3, tab->FindInPage(L"o", FWD, IGNORE_CASE, false, &ordinal));
  EXPECT_EQ(1, ordinal);
  // FindNext returns -1 for match count because it doesn't bother with
  // recounting the number of matches. We don't care about the match count
  // anyway in this case, we just want to check the ordinal.
  EXPECT_EQ(-1, tab->FindInPage(L"o", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"o", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Go back one match.
  EXPECT_EQ(-1, tab->FindInPage(L"o", BACK, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"o", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(3, ordinal);
  // This should wrap to the top.
  EXPECT_EQ(-1, tab->FindInPage(L"o", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(1, ordinal);
  // This should go back to the end.
  EXPECT_EQ(-1, tab->FindInPage(L"o", BACK, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// This test loads a page with frames and makes sure the ordinal returned makes
// sense.
TEST_F(FindInPageControllerTest, FindInPageMultiFramesOrdinal) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to our frames page.
  GURL url = server->TestServerPageW(kFramePage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  // Search for 'a', which should make the first item active and return
  // '1 in 7' (1st ordinal of a total of 7 matches).
  int ordinal = 0;
  EXPECT_EQ(7, tab->FindInPage(L"a", FWD, IGNORE_CASE, false, &ordinal));
  EXPECT_EQ(1, ordinal);
  // FindNext returns -1 for match count because it doesn't bother with
  // recounting the number of matches. We don't care about the match count
  // anyway in this case, we just want to check the ordinal.
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(4, ordinal);
  // Go back one, which should go back one frame.
  EXPECT_EQ(-1, tab->FindInPage(L"a", BACK, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(4, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(5, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(6, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(7, ordinal);
  // Now we should wrap back to frame 1.
  EXPECT_EQ(-1, tab->FindInPage(L"a", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(1, ordinal);
  // Now we should wrap back to frame last frame.
  EXPECT_EQ(-1, tab->FindInPage(L"a", BACK, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(7, ordinal);
}

// We could get ordinals out of whack when restarting search in subframes.
// See http://crbug.com/5132
TEST_F(FindInPageControllerTest, FindInPage_Issue5132) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to our frames page.
  GURL url = server->TestServerPageW(kFramePage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  // Search for 'goa' three times (6 matches on page).
  int ordinal = 0;
  EXPECT_EQ(6, tab->FindInPage(L"goa", FWD, IGNORE_CASE, false, &ordinal));
  EXPECT_EQ(1, ordinal);
  // FindNext returns -1 for match count because it doesn't bother with
  // recounting the number of matches. We don't care about the match count
  // anyway in this case, we just want to check the ordinal.
  EXPECT_EQ(-1, tab->FindInPage(L"goa", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(-1, tab->FindInPage(L"goa", FWD, IGNORE_CASE, true, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Add space to search (should result in no matches).
  EXPECT_EQ(0, tab->FindInPage(L"goa ", FWD, IGNORE_CASE, false, &ordinal));
  EXPECT_EQ(-1, ordinal);
  // Remove the space, should be back to '3 out of 6')
  EXPECT_EQ(6, tab->FindInPage(L"goa", FWD, IGNORE_CASE, false, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// Load a page with no selectable text and make sure we don't crash.
TEST_F(FindInPageControllerTest, FindUnSelectableText) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  GURL url = server->TestServerPageW(kUserSelectPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  EXPECT_EQ(0, tab->FindInPage(L"text", FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(0, tab->FindInPage(L"Non-existing string", FWD, IGNORE_CASE,
                               false, NULL));
}

// Try to reproduce the crash seen in issue 1341577.
TEST_F(FindInPageControllerTest, FindCrash_Issue1341577) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  GURL url = server->TestServerPageW(kCrashPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  // This would crash the tab. These must be the first two find requests issued
  // against the frame, otherwise an active frame pointer is set and it wont
  // produce the crash.
  EXPECT_EQ(1, tab->FindInPage(L"\u0D4C", FWD, IGNORE_CASE, false, NULL));
  // FindNext returns -1 for match count because it doesn't bother with
  // recounting the number of matches. We don't care about the match count
  // anyway in this case, we just want to make sure it doesn't crash.
  EXPECT_EQ(-1, tab->FindInPage(L"\u0D4C", FWD, IGNORE_CASE, true, NULL));

  // This should work fine.
  EXPECT_EQ(1, tab->FindInPage(L"\u0D24\u0D46", FWD, IGNORE_CASE, false, NULL));
  EXPECT_EQ(0, tab->FindInPage(L"nostring", FWD, IGNORE_CASE, false, NULL));
}

// Test to make sure Find does the right thing when restarting from a timeout.
// We used to have a problem where we'd stop finding matches when all of the
// following conditions were true:
// 1) The page has a lot of text to search.
// 2) The page contains more than one match.
// 3) It takes longer than the time-slice given to each Find operation (100
//    ms) to find one or more of those matches (so Find times out and has to try
//    again from where it left off).
TEST_F(FindInPageControllerTest, FindEnoughMatches_Issue1155639) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  GURL url = server->TestServerPageW(kTooFewMatchesPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  // This string appears 5 times at the bottom of a long page. If Find restarts
  // properly after a timeout, it will find 5 matches, not just 1.
  EXPECT_EQ(5, tab->FindInPage(L"008.xml", FWD, IGNORE_CASE, false, NULL));
}

// The find window should not change its location just because we open and close
// a new tab.
TEST_F(FindInPageControllerTest, FindMovesOnTabClose_Issue1343052) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  GURL url = server->TestServerPageW(kFramePage);
  scoped_ptr<TabProxy> tabA(GetActiveTab());
  ASSERT_TRUE(tabA->NavigateToURL(url));
  WaitUntilTabCount(1);

  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get() != NULL);

  // Toggle the bookmark bar state.
  browser->ApplyAccelerator(IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(WaitForBookmarkBarVisibilityChange(browser.get(), true));

  // Open the Find window and wait for it to animate.
  EXPECT_TRUE(tabA->OpenFindInPage());
  EXPECT_TRUE(WaitForFindWindowVisibilityChange(tabA.get(), true));

  // Find its location.
  int x = -1, y = -1;
  EXPECT_TRUE(tabA->GetFindWindowLocation(&x, &y));

  // Open another tab (tab B).
  EXPECT_TRUE(browser->AppendTab(url));
  scoped_ptr<TabProxy> tabB(GetActiveTab());

  // Close tab B.
  EXPECT_TRUE(tabB->Close(true));

  // See if the Find window has moved.
  int new_x = -1, new_y = -1;
  EXPECT_TRUE(tabA->GetFindWindowLocation(&new_x, &new_y));

  EXPECT_EQ(x, new_x);
  EXPECT_EQ(y, new_y);

  // Now reset the bookmark bar state and try the same again.
  browser->ApplyAccelerator(IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(WaitForBookmarkBarVisibilityChange(browser.get(), false));

  // Bookmark bar has moved, reset our coordinates.
  EXPECT_TRUE(tabA->GetFindWindowLocation(&x, &y));

  // Open another tab (tab C).
  EXPECT_TRUE(browser->AppendTab(url));
  scoped_ptr<TabProxy> tabC(GetActiveTab());

  // Close it.
  EXPECT_TRUE(tabC->Close(true));

  // See if the Find window has moved.
  EXPECT_TRUE(tabA->GetFindWindowLocation(&new_x, &new_y));

  EXPECT_EQ(x, new_x);
  EXPECT_EQ(y, new_y);
}

// Make sure Find box disappears on Navigate but not on Refresh.
TEST_F(FindInPageControllerTest, FindDisappearOnNavigate) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data");
  ASSERT_TRUE(NULL != server.get());

  GURL url = server->TestServerPageW(kUserSelectPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab->NavigateToURL(url));
  WaitUntilTabCount(1);

  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get() != NULL);

  // Open the Find window and wait for it to animate.
  EXPECT_TRUE(tab->OpenFindInPage());
  EXPECT_TRUE(WaitForFindWindowVisibilityChange(tab.get(), true));

  // Reload the tab and make sure Find box doesn't go away.
  EXPECT_TRUE(tab->Reload());
  EXPECT_TRUE(WaitForFindWindowVisibilityChange(tab.get(), true));

  // Navigate and make sure the Find box goes away.
  EXPECT_TRUE(tab->NavigateToURL(url));
  EXPECT_TRUE(WaitForFindWindowVisibilityChange(tab.get(), false));
}
