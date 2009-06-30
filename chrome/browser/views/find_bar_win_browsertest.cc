// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/views/find_bar_win.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/focus/focus_manager.h"

const std::wstring kSimplePage = L"404_is_enough_for_us.html";
const std::wstring kFramePage = L"files/find_in_page/frames.html";
const std::wstring kFrameData = L"files/find_in_page/framedata_general.html";
const std::wstring kUserSelectPage = L"files/find_in_page/user-select.html";
const std::wstring kCrashPage = L"files/find_in_page/crash_1341577.html";
const std::wstring kTooFewMatchesPage = L"files/find_in_page/bug_1155639.html";
const std::wstring kEndState = L"files/find_in_page/end_state.html";
const std::wstring kPrematureEnd = L"files/find_in_page/premature_end.html";
const std::wstring kMoveIfOver = L"files/find_in_page/move_if_obscuring.html";
const std::wstring kBitstackCrash = L"files/find_in_page/crash_14491.html";

class FindInPageNotificationObserver : public NotificationObserver {
 public:
  explicit FindInPageNotificationObserver(TabContents* parent_tab)
      : parent_tab_(parent_tab),
        active_match_ordinal_(-1),
        number_of_matches_(0) {
    current_find_request_id_ = parent_tab->current_find_request_id();
    registrar_.Add(this, NotificationType::FIND_RESULT_AVAILABLE,
        Source<TabContents>(parent_tab_));
    ui_test_utils::RunMessageLoop();
  }

  int active_match_ordinal() const { return active_match_ordinal_; }

  int number_of_matches() const { return number_of_matches_; }

  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::FIND_RESULT_AVAILABLE) {
      Details<FindNotificationDetails> find_details(details);
      if (find_details->request_id() == current_find_request_id_) {
        // We get multiple responses and one of those will contain the ordinal.
        // This message comes to us before the final update is sent.
        if (find_details->active_match_ordinal() > -1)
          active_match_ordinal_ = find_details->active_match_ordinal();
        if (find_details->final_update()) {
          number_of_matches_ = find_details->number_of_matches();
          MessageLoopForUI::current()->Quit();
        } else {
          DLOG(INFO) << "Ignoring, since we only care about the final message";
        }
      }
    } else {
      NOTREACHED();
    }
  }

 private:
  NotificationRegistrar registrar_;
  TabContents* parent_tab_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
  int number_of_matches_;
  // The id of the current find request, obtained from TabContents. Allows us
  // to monitor when the search completes.
  int current_find_request_id_;
};

typedef enum { BACK = 0, FWD = 1 } FindInPageDirection;
typedef enum { IGNORE_CASE = 0, CASE_SENSITIVE = 1 } FindInPageCase;

class FindInPageControllerTest : public InProcessBrowserTest {
 public:
  FindInPageControllerTest() {
    EnableDOMAutomation();
  }

 protected:
  int FindInPage(const std::wstring& search_string,
                 FindInPageDirection forward,
                 FindInPageCase match_case,
                 int* ordinal) {
    TabContents* tab_contents = browser()->GetSelectedTabContents();
    tab_contents->StartFinding(search_string, forward == FWD,
                               match_case == CASE_SENSITIVE);

    FindInPageNotificationObserver observer =
        FindInPageNotificationObserver(tab_contents);
    if (ordinal)
      *ordinal = observer.active_match_ordinal();
    return observer.number_of_matches();
  }

  void GetFindBarWindowInfo(gfx::Point* position, bool* fully_visible) {
    FindBarTesting* find_bar =
        browser()->find_bar()->find_bar()->GetFindBarTesting();
    find_bar->GetFindBarWindowInfo(position, fully_visible);
  }
};

// This test loads a page with frames and starts FindInPage requests.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageFrames) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our frames page.
  GURL url = server->TestServerPageW(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Try incremental search (mimicking user typing in).
  int ordinal = 0;
  EXPECT_EQ(18, FindInPage(L"g",       FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(11, FindInPage(L"go",      FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(04, FindInPage(L"goo",     FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(03, FindInPage(L"goog",    FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(02, FindInPage(L"googl",   FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(01, FindInPage(L"google",  FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(00, FindInPage(L"google!", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Negative test (no matches should be found).
  EXPECT_EQ(0, FindInPage(L"Non-existing string", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);

  // 'horse' only exists in the three right frames.
  EXPECT_EQ(3, FindInPage(L"horse", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // 'cat' only exists in the first frame.
  EXPECT_EQ(1, FindInPage(L"cat", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try searching again, should still come up with 1 match.
  EXPECT_EQ(1, FindInPage(L"cat", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try searching backwards, ignoring case, should still come up with 1 match.
  EXPECT_EQ(1, FindInPage(L"CAT", BACK, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try case sensitive, should NOT find it.
  EXPECT_EQ(0, FindInPage(L"CAT", FWD, CASE_SENSITIVE, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Try again case sensitive, but this time with right case.
  EXPECT_EQ(1, FindInPage(L"dog", FWD, CASE_SENSITIVE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try non-Latin characters ('Hreggvidur' with 'eth' for 'd' in left frame).
  EXPECT_EQ(1, FindInPage(L"Hreggvi\u00F0ur", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(1, FindInPage(L"Hreggvi\u00F0ur", FWD, CASE_SENSITIVE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0, FindInPage(L"hreggvi\u00F0ur", FWD, CASE_SENSITIVE, &ordinal));
  EXPECT_EQ(0, ordinal);
}

std::string FocusedOnPage(TabContents* tab_contents) {
  std::string result;
  ui_test_utils::ExecuteJavaScriptAndExtractString(
      tab_contents,
      L"",
      L"window.domAutomationController.send(getFocusedElement());",
      &result);
  return result;
}

// This tests the FindInPage end-state, in other words: what is focused when you
// close the Find box (ie. if you find within a link the link should be
// focused).
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageEndState) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our special focus tracking page.
  GURL url = server->TestServerPageW(kEndState);
  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(NULL != tab_contents);

  // Verify that nothing has focus.
  ASSERT_STREQ("{nothing focused}", FocusedOnPage(tab_contents).c_str());

  // Search for a text that exists within a link on the page.
  int ordinal = 0;
  EXPECT_EQ(1, FindInPage(L"nk", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // End the find session, which should set focus to the link.
  tab_contents->StopFinding(false);

  // Verify that the link is focused.
  EXPECT_STREQ("link1", FocusedOnPage(tab_contents).c_str());

  // Search for a text that exists within a link on the page.
  EXPECT_EQ(1, FindInPage(L"Google", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Move the selection to link 1, after searching.
  std::string result;
  ui_test_utils::ExecuteJavaScriptAndExtractString(
      tab_contents,
      L"",
      L"window.domAutomationController.send(selectLink1());",
      &result);

  // End the find session.
  tab_contents->StopFinding(false);

  // Verify that link2 is not focused.
  EXPECT_STREQ("", FocusedOnPage(tab_contents).c_str());
}

// This test loads a single-frame page and makes sure the ordinal returned makes
// sense as we FindNext over all the items.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageOrdinal) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our page.
  GURL url = server->TestServerPageW(kFrameData);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'o', which should make the first item active and return
  // '1 in 3' (1st ordinal of a total of 3 matches).
  int ordinal = 0;
  EXPECT_EQ(3, FindInPage(L"o", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(3, FindInPage(L"o", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(3, FindInPage(L"o", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Go back one match.
  EXPECT_EQ(3, FindInPage(L"o", BACK, IGNORE_CASE, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(3, FindInPage(L"o", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(3, ordinal);
  // This should wrap to the top.
  EXPECT_EQ(3, FindInPage(L"o", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  // This should go back to the end.
  EXPECT_EQ(3, FindInPage(L"o", BACK, IGNORE_CASE, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// This test loads a page with frames and makes sure the ordinal returned makes
// sense.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageMultiFramesOrdinal) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our page.
  GURL url = server->TestServerPageW(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'a', which should make the first item active and return
  // '1 in 7' (1st ordinal of a total of 7 matches).
  int ordinal = 0;
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(4, ordinal);
  // Go back one, which should go back one frame.
  EXPECT_EQ(7, FindInPage(L"a", BACK, IGNORE_CASE, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(4, ordinal);
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(5, ordinal);
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(6, ordinal);
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(7, ordinal);
  // Now we should wrap back to frame 1.
  EXPECT_EQ(7, FindInPage(L"a", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  // Now we should wrap back to frame last frame.
  EXPECT_EQ(7, FindInPage(L"a", BACK, IGNORE_CASE, &ordinal));
  EXPECT_EQ(7, ordinal);
}

// We could get ordinals out of whack when restarting search in subframes.
// See http://crbug.com/5132
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPage_Issue5132) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our page.
  GURL url = server->TestServerPageW(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'goa' three times (6 matches on page).
  int ordinal = 0;
  EXPECT_EQ(6, FindInPage(L"goa", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(6, FindInPage(L"goa", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(6, FindInPage(L"goa", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Add space to search (should result in no matches).
  EXPECT_EQ(0, FindInPage(L"goa ", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);
  // Remove the space, should be back to '3 out of 6')
  EXPECT_EQ(6, FindInPage(L"goa", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// Load a page with no selectable text and make sure we don't crash.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindUnSelectableText) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our page.
  GURL url = server->TestServerPageW(kUserSelectPage);
  ui_test_utils::NavigateToURL(browser(), url);

  int ordinal = 0;
  EXPECT_EQ(0, FindInPage(L"text", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(-1, ordinal);  // Nothing is selected.
  EXPECT_EQ(0, FindInPage(L"Non-existing string", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Try to reproduce the crash seen in issue 1341577.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindCrash_Issue1341577) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our page.
  GURL url = server->TestServerPageW(kCrashPage);
  ui_test_utils::NavigateToURL(browser(), url);

  // This would crash the tab. These must be the first two find requests issued
  // against the frame, otherwise an active frame pointer is set and it wont
  // produce the crash.
  int ordinal = 0;
  EXPECT_EQ(1, FindInPage(L"\u0D4C", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(1, FindInPage(L"\u0D4C", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // This should work fine.
  EXPECT_EQ(1, FindInPage(L"\u0D24\u0D46", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0, FindInPage(L"nostring", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Try to reproduce the crash seen in http://crbug.com/14491, where an assert
// hits in the BitStack size comparison in WebKit.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindCrash_Issue14491) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our page.
  GURL url = server->TestServerPageW(kBitstackCrash);
  ui_test_utils::NavigateToURL(browser(), url);

  // This used to crash the tab.
  int ordinal = 0;
  EXPECT_EQ(0, FindInPage(L"s", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Test to make sure Find does the right thing when restarting from a timeout.
// We used to have a problem where we'd stop finding matches when all of the
// following conditions were true:
// 1) The page has a lot of text to search.
// 2) The page contains more than one match.
// 3) It takes longer than the time-slice given to each Find operation (100
//    ms) to find one or more of those matches (so Find times out and has to try
//    again from where it left off).
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindRestarts_Issue1155639) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our page.
  GURL url = server->TestServerPageW(kTooFewMatchesPage);
  ui_test_utils::NavigateToURL(browser(), url);

  // This string appears 5 times at the bottom of a long page. If Find restarts
  // properly after a timeout, it will find 5 matches, not just 1.
  int ordinal = 0;
  EXPECT_EQ(5, FindInPage(L"008.xml", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// This tests bug 11761: FindInPage terminates search prematurely.
// This test will be enabled once the bug is fixed.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       DISABLED_FindInPagePrematureEnd) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our special focus tracking page.
  GURL url = server->TestServerPageW(kPrematureEnd);
  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(NULL != tab_contents);

  // Search for a text that exists within a link on the page.
  int ordinal = 0;
  EXPECT_EQ(2, FindInPage(L"html ", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// Make sure Find box disappears on Navigate but not on Refresh.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindDisappearOnNavigate) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our special focus tracking page.
  GURL url = server->TestServerPageW(kSimplePage);
  GURL url2 = server->TestServerPageW(kFramePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open the Find window with animations disabled.
  FindBarWin::disable_animations_during_testing_ = true;
  browser()->ShowFindBar();

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_TRUE(fully_visible);

  // Reload the tab and make sure Find window doesn't go away.
  browser()->Reload();

  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_TRUE(fully_visible);

  // Navigate and make sure the Find window goes away.
  ui_test_utils::NavigateToURL(browser(), url2);

  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_FALSE(fully_visible);
}

// Make sure Find box disappears when History/Downloads page is opened, and
// when a New Tab is opened.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       FindDisappearOnNewTabAndHistory) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our special focus tracking page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open the Find window with animations disabled.
  FindBarWin::disable_animations_during_testing_ = true;
  browser()->ShowFindBar();

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_TRUE(fully_visible);

  // Open another tab (tab B).
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  // Make sure Find box is closed.
  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_FALSE(fully_visible);

  // Close tab B.
  browser()->CloseTab();

  // Make sure Find window appears again.
  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_TRUE(fully_visible);

  browser()->ShowHistoryTab();

  // Make sure Find box is closed.
  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_FALSE(fully_visible);
}

// Make sure Find box moves out of the way if it is obscuring the active match.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindMovesWhenObscuring) {
  HTTPTestServer* server = StartHTTPServer();

  GURL url = server->TestServerPageW(kMoveIfOver);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open the Find window with animations disabled.
  FindBarWin::disable_animations_during_testing_ = true;
  browser()->ShowFindBar();

  gfx::Point start_position;
  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  GetFindBarWindowInfo(&start_position, &fully_visible);
  EXPECT_TRUE(fully_visible);

  // Search for 'dream' which the Find box is obscuring.
  int ordinal = 0;
  EXPECT_EQ(1, FindInPage(L"dream", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Make sure Find box has moved.
  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_EQ(start_position.y(), position.y());
  EXPECT_NE(start_position.x(), position.x());
  EXPECT_TRUE(fully_visible);

  // Search for 'Too much' which the Find box is not obscuring.
  EXPECT_EQ(1, FindInPage(L"Too much", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Make sure Find box has moved back to its original location.
  GetFindBarWindowInfo(&position, &fully_visible);
  EXPECT_EQ(start_position, position);
  EXPECT_TRUE(fully_visible);
}

// Make sure F3 in a new tab works if Find has previous string to search for.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       FindNextInNewTabUsesPrepopulate) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to any page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Search for 'no_match'. No matches should be found.
  int ordinal = 0;
  EXPECT_EQ(0, FindInPage(L"no_match", FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Open another tab (tab B).
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  // Simulate what happens when you press F3 for FindNext. We should get a
  // response here (a hang means search was aborted).
  EXPECT_EQ(0, FindInPage(std::wstring(), FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Open another tab (tab C).
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), url);

  // Simulate what happens when you press F3 for FindNext. We should get a
  // response here (a hang means search was aborted).
  EXPECT_EQ(0, FindInPage(std::wstring(), FWD, IGNORE_CASE, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Make sure Find box grabs the Esc accelerator and restores it again.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, AcceleratorRestoring) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to any page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  gfx::NativeView browser_view = browser()->window()->GetNativeHandle();
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManagerForNativeView(browser_view);

  // See where Escape is registered.
  views::Accelerator escape(VK_ESCAPE, false, false, false);
  views::AcceleratorTarget* old_target =
      focus_manager->GetCurrentTargetForAccelerator(escape);
  EXPECT_TRUE(old_target != NULL);

  // Open the Find box.
  browser()->ShowFindBar();

  // Our Find bar should be the new target.
  views::AcceleratorTarget* new_target =
      focus_manager->GetCurrentTargetForAccelerator(escape);

  EXPECT_TRUE(new_target != NULL);
  EXPECT_NE(new_target, old_target);

  // Close the Find box.
  browser()->find_bar()->EndFindSession();

  // The accelerator for Escape should be back to what it was before.
  EXPECT_EQ(old_target, focus_manager->GetCurrentTargetForAccelerator(escape));
}

// Make sure Find box does not become UI-inactive when no text is in the box as
// we switch to a tab contents with an empty find string. See issue 13570.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, StayActive) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to any page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open the Find window with animations disabled.
  FindBarWin::disable_animations_during_testing_ = true;
  browser()->ShowFindBar();

  // Simulate a user clearing the search string. Ideally, we should be
  // simulating keypresses here for searching for something and pressing
  // backspace, but that's been proven flaky in the past, so we go straight to
  // tab_contents.
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  // Stop the (non-existing) find operation, and clear the selection (which
  // signals the UI is still active).
  tab_contents->StopFinding(true);
  // Make sure the Find UI flag hasn't been cleared, it must be so that the UI
  // still responds to browser window resizing.
  ASSERT_TRUE(tab_contents->find_ui_active());
}
