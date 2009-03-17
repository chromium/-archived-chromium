// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/views/focus/focus_manager.h"
#include "chrome/views/view.h"
#include "chrome/views/window/window.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"


namespace {

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

const wchar_t kSimplePage[] = L"files/focus/page_with_focus.html";
const wchar_t kStealFocusPage[] = L"files/focus/page_steals_focus.html";
const wchar_t kTypicalPage[] = L"files/focus/typical_page.html";

class BrowserFocusTest : public InProcessBrowserTest {
 public:
  BrowserFocusTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }
};

class JavaScriptRunner : public NotificationObserver {
 public:
  JavaScriptRunner(WebContents* web_contents,
                   const std::wstring& frame_xpath,
                   const std::wstring& jscript)
      : web_contents_(web_contents),
        frame_xpath_(frame_xpath),
        jscript_(jscript) {
    NotificationService::current()->
        AddObserver(this, NotificationType::DOM_OPERATION_RESPONSE,
                    Source<WebContents>(web_contents));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    Details<DomOperationNotificationDetails> dom_op_details(details);
    result_ = dom_op_details->json();
    // The Jasonified response has quotes, remove them.
    if (result_.length() > 1 && result_[0] == '"')
      result_ = result_.substr(1, result_.length() - 2);

    NotificationService::current()->
        RemoveObserver(this, NotificationType::DOM_OPERATION_RESPONSE,
                       Source<WebContents>(web_contents_));
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  std::string Run() {
    // The DOMAutomationController requires an automation ID, eventhough we are
    // not using it in this case.
    web_contents_->render_view_host()->ExecuteJavascriptInWebFrame(
        frame_xpath_, L"window.domAutomationController.setAutomationId(0);");

    web_contents_->render_view_host()->ExecuteJavascriptInWebFrame(frame_xpath_,
                                                                   jscript_);
    ui_test_utils::RunMessageLoop();
    return result_;
  }

 private:
  WebContents* web_contents_;
  std::wstring frame_xpath_;
  std::wstring jscript_;
  std::string result_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptRunner);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BrowsersRememberFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // The focus should be on the Tab contents.
  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForHWND(hwnd);
  ASSERT_TRUE(browser_view);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(hwnd);
  ASSERT_TRUE(focus_manager);

  EXPECT_EQ(browser_view->GetContentsView(), focus_manager->GetFocusedView());

  // Now hide the window, show it again, the focus should not have changed.
  // TODO(jcampan): retrieve the WidgetWin and show/hide on it instead of
  // using Windows API.
  ::ShowWindow(hwnd, SW_HIDE);
  ::ShowWindow(hwnd, SW_SHOW);
  EXPECT_EQ(browser_view->GetContentsView(), focus_manager->GetFocusedView());

  // Click on the location bar.
  LocationBarView* location_bar = browser_view->GetLocationBarView();
  ui_controls::MoveMouseToCenterAndPress(location_bar,
                                         ui_controls::LEFT,
                                         ui_controls::DOWN | ui_controls::UP,
                                         new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  // Location bar should have focus.
  EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

  // Hide the window, show it again, the focus should not have changed.
  ::ShowWindow(hwnd, SW_HIDE);
  ::ShowWindow(hwnd, SW_SHOW);
  EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

  // Open a new browser window.
  Browser* browser2 = Browser::Create(browser()->profile());
  ASSERT_TRUE(browser2);
  browser2->AddBlankTab(true);
  browser2->window()->Show();
  ui_test_utils::NavigateToURL(browser2, url);

  HWND hwnd2 = reinterpret_cast<HWND>(browser2->window()->GetNativeHandle());
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForHWND(hwnd2);
  ASSERT_TRUE(browser_view2);
  views::FocusManager* focus_manager2 =
      views::FocusManager::GetFocusManager(hwnd2);
  ASSERT_TRUE(focus_manager2);
  EXPECT_EQ(browser_view2->GetContentsView(), focus_manager2->GetFocusedView());

  // Switch to the 1st browser window, focus should still be on the location
  // bar and the second browser should have nothing focused.
  browser()->window()->Activate();
  EXPECT_EQ(location_bar, focus_manager->GetFocusedView());
  EXPECT_EQ(NULL, focus_manager2->GetFocusedView());

  // Switch back to the second browser, focus should still be on the page.
  browser2->window()->Activate();
  EXPECT_EQ(NULL, focus_manager->GetFocusedView());
  EXPECT_EQ(browser_view2->GetContentsView(), focus_manager2->GetFocusedView());

  // Close the 2nd browser to avoid a DCHECK().
  browser_view2->Close();
}

// Tabs remember focus.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabsRememberFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForHWND(hwnd);
  ASSERT_TRUE(browser_view);

  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(hwnd);
  ASSERT_TRUE(focus_manager);

  // Create several tabs.
  for (int i = 0; i < 4; ++i)
    browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED, true, NULL);

  // Alternate focus for the tab.
  const bool kFocusPage[3][5] = {
    { true, true, true, true, false },
    { false, false, false, false, false },
    { false, true, false, true, false }
  };

  for (int i = 1; i < 3; i++) {
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->SelectTabContentsAt(j, true);

      // Activate the location bar or the page.
      views::View* view_to_focus = kFocusPage[i][j] ?
          browser_view->GetContentsView() :
          browser_view->GetLocationBarView();

      ui_controls::MoveMouseToCenterAndPress(view_to_focus,
                                             ui_controls::LEFT,
                                             ui_controls::DOWN |
                                                 ui_controls::UP,
                                             new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
    }

    // Now come back to the tab and check the right view is focused.
    for (int j = 0; j < 5; j++) {
      // Activate the tab.
      browser()->SelectTabContentsAt(j, true);

      // Activate the location bar or the page.
      views::View* view = kFocusPage[i][j] ?
          browser_view->GetContentsView() :
          browser_view->GetLocationBarView();
      EXPECT_EQ(view, focus_manager->GetFocusedView());
    }
  }
}

// Background window does not steal focus.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, BackgroundBrowserDontStealFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Open a new browser window.
  Browser* browser2 = Browser::Create(browser()->profile());
  ASSERT_TRUE(browser2);
  browser2->AddBlankTab(true);
  browser2->window()->Show();
  GURL steal_focus_url = server->TestServerPageW(kStealFocusPage);
  ui_test_utils::NavigateToURL(browser2, steal_focus_url);

  // Activate the first browser.
  browser()->window()->Activate();

  // Wait for the focus to be stolen by the other browser.
  ::Sleep(2000);

  // Make sure the first browser is still active.
  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForHWND(hwnd);
  ASSERT_TRUE(browser_view);
  EXPECT_TRUE(browser_view->frame()->IsActive());

  // Close the 2nd browser to avoid a DCHECK().
  HWND hwnd2 = reinterpret_cast<HWND>(browser2->window()->GetNativeHandle());
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForHWND(hwnd2);
  browser_view2->Close();
}

// Page cannot steal focus when focus is on location bar.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, LocationBarLockFocus) {
  HTTPTestServer* server = StartHTTPServer();

  // Open the page that steals focus.
  GURL url = server->TestServerPageW(kStealFocusPage);
  ui_test_utils::NavigateToURL(browser(), url);

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForHWND(hwnd);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(hwnd);

  // Click on the location bar.
  LocationBarView* location_bar = browser_view->GetLocationBarView();
  ui_controls::MoveMouseToCenterAndPress(location_bar,
                                         ui_controls::LEFT,
                                         ui_controls::DOWN | ui_controls::UP,
                                         new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // Wait for the page to steal focus.
  ::Sleep(2000);

  // Make sure the location bar is still focused.
  EXPECT_EQ(location_bar, focus_manager->GetFocusedView());
}

// Focus traversal
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FocusTraversal) {
  HTTPTestServer* server = StartHTTPServer();

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForHWND(hwnd);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(hwnd);

  // Click on the location bar.
  LocationBarView* location_bar = browser_view->GetLocationBarView();
  ui_controls::MoveMouseToCenterAndPress(location_bar,
                                         ui_controls::LEFT,
                                         ui_controls::DOWN | ui_controls::UP,
                                         new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  const char* kExpElementIDs[] = {
    "",  // Initially no element in the page should be focused
         // (the location bar is focused).
    "textEdit", "searchButton", "luckyButton", "googleLink", "gmailLink",
    "gmapLink"
  };

  // Test forward focus traversal.
  for (int i = 0; i < 3; ++i) {
    // Location bar should be focused.
    EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

    // Now let's press tab to move the focus.
    for (int j = 0; j < 7; ++j) {
      // Let's make sure the focus is on the expected element in the page.
      JavaScriptRunner js_runner(
          browser()->GetSelectedTabContents()->AsWebContents(),
          L"",
          L"window.domAutomationController.send(getFocusedElement());");
      std::string actual = js_runner.Run();
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      ui_controls::SendKeyPressNotifyWhenDone(L'\t', false, false, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
      // Ideally, we wouldn't sleep here and instead would use the event
      // processed ack nofification from the renderer.  I am reluctant to create
      // a new notification/callback for that purpose just for this test.
      ::Sleep(kActionDelayMs);
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 3; ++i) {
    // Location bar should be focused.
    EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

    // Now let's press shift-tab to move the focus in reverse.
    for (int j = 0; j < 7; ++j) {
      ui_controls::SendKeyPressNotifyWhenDone(L'\t', false, true, false,
                                              new MessageLoop::QuitTask());
      ui_test_utils::RunMessageLoop();
      ::Sleep(kActionDelayMs);

      // Let's make sure the focus is on the expected element in the page.
      JavaScriptRunner js_runner(
          browser()->GetSelectedTabContents()->AsWebContents(),
          L"",
          L"window.domAutomationController.send(getFocusedElement());");
      std::string actual = js_runner.Run();
      ASSERT_STREQ(kExpElementIDs[6 - j], actual.c_str());
    }

    // At this point the renderer has sent us a message asking to advance the
    // focus (as the end of the focus loop was reached in the renderer).
    // We need to run the message loop to process it.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }
}

// Make sure Find box can request focus, even when it is already open.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, FindFocusTest) {
  HTTPTestServer* server = StartHTTPServer();

  // Open some page (any page that doesn't steal focus).
  GURL url = server->TestServerPageW(kTypicalPage);
  ui_test_utils::NavigateToURL(browser(), url);

  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForHWND(hwnd);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(hwnd);
  LocationBarView* location_bar = browser_view->GetLocationBarView();

  // Press Ctrl+F, which will make the Find box open and request focus.
  static const int VK_F = 0x46;
  ui_controls::SendKeyPressNotifyWhenDone(L'F', true, false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // Ideally, we wouldn't sleep here and instead would intercept the
  // RenderViewHostDelegate::HandleKeyboardEvent() callback.  To do that, we
  // could create a RenderViewHostDelegate wrapper and hook-it up by either:
  // - creating a factory used to create the delegate
  // - making the test a private and overwriting the delegate member directly.
  ::Sleep(kActionDelayMs);
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  views::View* focused_view = focus_manager->GetFocusedView();
  ASSERT_TRUE(focused_view != NULL);
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view->GetID());

  // Click on the location bar.
  ui_controls::MoveMouseToCenterAndPress(location_bar,
                                         ui_controls::LEFT,
                                         ui_controls::DOWN | ui_controls::UP,
                                         new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // Make sure the location bar is focused.
  EXPECT_EQ(location_bar, focus_manager->GetFocusedView());

  // Now press Ctrl+F again and focus should move to the Find box.
  ui_controls::SendKeyPressNotifyWhenDone(L'F', true, false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  focused_view = focus_manager->GetFocusedView();
  ASSERT_TRUE(focused_view != NULL);
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view->GetID());

  // Set focus to the page.
  ui_controls::MoveMouseToCenterAndPress(browser_view->GetContentsView(),
                                         ui_controls::LEFT,
                                         ui_controls::DOWN | ui_controls::UP,
                                         new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ(browser_view->GetContentsView(), focus_manager->GetFocusedView());

  // Now press Ctrl+F again and focus should move to the Find box.
  ui_controls::SendKeyPressNotifyWhenDone(VK_F, true, false, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // See remark above on why we wait.
  ::Sleep(kActionDelayMs);
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  focused_view = focus_manager->GetFocusedView();
  ASSERT_TRUE(focused_view != NULL);
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view->GetID());
}

// Makes sure the focus is in the right location when opening the different
// types of tabs.
IN_PROC_BROWSER_TEST_F(BrowserFocusTest, TabInitialFocus) {
  HWND hwnd = reinterpret_cast<HWND>(browser()->window()->GetNativeHandle());
  BrowserView* browser_view = BrowserView::GetBrowserViewForHWND(hwnd);
  ASSERT_TRUE(browser_view);
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(hwnd);
  ASSERT_TRUE(focus_manager);

  // Open the history tab, focus should be on the tab contents.
  browser()->ShowHistoryTab();
  EXPECT_EQ(browser_view->GetContentsView(), focus_manager->GetFocusedView());

  // Open the new tab, focus should be on the location bar.
  browser()->NewTab();
  EXPECT_EQ(browser_view->GetLocationBarView(),
            focus_manager->GetFocusedView());

  // Open the download tab, focus should be on the tab contents.
  browser()->ShowDownloadsTab();
  EXPECT_EQ(browser_view->GetContentsView(), focus_manager->GetFocusedView());
}
