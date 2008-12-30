// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/view_ids.h"
#include "chrome/views/view.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

namespace {

// The delay waited after sending an OS simulated event.
const int kActionDelayMs = 500;

const wchar_t kDocRoot[] = L"chrome/test/data";

const wchar_t kSimplePage[] = L"files/focus/page_with_focus.html";
const wchar_t kStealFocusPage[] = L"files/focus/page_steals_focus.html";
const wchar_t kTypicalPage[] = L"files/focus/typical_page.html";

class BrowserFocusTest : public UITest {
 public:
  BrowserFocusTest() {
    show_window_ = true;
    dom_automation_enabled_ = true;
  }
};

// Activate a tab by clicking on it.  Returns true if the call was successful
// (meaning the messages were correctly sent, but does not guarantee the tab
// has been changed).
bool ActivateTabByClick(AutomationProxy* automation,
                        WindowProxy* browser_window,
                        int tab_index) {
  // Click on the tab.
  gfx::Rect bounds;

  if (!browser_window->GetViewBounds(VIEW_ID_TAB_0 + tab_index, &bounds, true))
    return false;

  POINT click(bounds.CenterPoint().ToPOINT());
  if (!browser_window->SimulateOSClick(click,
                                       views::Event::EF_LEFT_BUTTON_DOWN))
    return false;

  // Wait a bit to let the click be processed.
  ::Sleep(kActionDelayMs);

  return true;
}

}  // namespace

TEST_F(BrowserFocusTest, BrowsersRememberFocus) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  EXPECT_NE(AUTOMATION_MSG_NAVIGATION_ERROR, tab->NavigateToURL(url));

  // The focus should be on the Tab contents.
  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get() != NULL);

  scoped_ptr<BrowserProxy> browser(automation()->
      GetBrowserForWindow(window.get()));
  ASSERT_TRUE(browser.get() != NULL);

  int focused_view_id;
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_TAB_CONTAINER, focused_view_id);

  // Now hide the window, show it again, the focus should not have changed.
  EXPECT_TRUE(window->SetVisible(false));
  EXPECT_TRUE(window->SetVisible(true));
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_TAB_CONTAINER, focused_view_id);

  // Click on the location bar.
  gfx::Rect bounds;
  EXPECT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &bounds, false));
  POINT click(bounds.CenterPoint().ToPOINT());

  EXPECT_TRUE(window->SimulateOSClick(click,
      views::Event::EF_LEFT_BUTTON_DOWN));
  ::Sleep(kActionDelayMs);
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);

  // Hide the window, show it again, the focus should not have changed.
  EXPECT_TRUE(window->SetVisible(false));
  EXPECT_TRUE(window->SetVisible(true));
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);

  // Open a new browser window.
  EXPECT_TRUE(automation()->OpenNewBrowserWindow(SW_SHOWNORMAL));
  scoped_ptr<WindowProxy> new_window(automation()->GetActiveWindow());
  ASSERT_TRUE(new_window.get() != NULL);
  scoped_ptr<BrowserProxy> new_browser(automation()->
      GetBrowserForWindow(new_window.get()));
  ASSERT_TRUE(new_browser.get() != NULL);

  // Let's make sure we have 2 different browser windows.
  EXPECT_TRUE(browser->handle() != new_browser->handle());

  tab.reset(new_browser->GetActiveTab());
  EXPECT_TRUE(tab.get());
  tab->NavigateToURL(url);

  // Switch to the 1st browser window, focus should still be on the location
  // bar and the second browser should have nothing focused.
  EXPECT_TRUE(window->Activate());
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);
  EXPECT_TRUE(new_window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(-1, focused_view_id);

  // Switch back to the second browser, focus should still be on the page.
  EXPECT_TRUE(new_window->Activate());
  EXPECT_TRUE(new_window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_TAB_CONTAINER, focused_view_id);
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(-1, focused_view_id);
}

// Tabs remember focus.
TEST_F(BrowserFocusTest, TabsRememberFocus) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());

  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get() != NULL);
  scoped_ptr<BrowserProxy> browser(
      automation()->GetBrowserForWindow(window.get()));
  ASSERT_TRUE(browser.get() != NULL);

  // First we navigate to our test page.
  GURL url = server->TestServerPageW(kSimplePage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  tab->NavigateToURL(url);

  // Create several tabs.
  EXPECT_TRUE(browser->AppendTab(url));
  EXPECT_TRUE(browser->AppendTab(url));
  EXPECT_TRUE(browser->AppendTab(url));
  EXPECT_TRUE(browser->AppendTab(url));

  int tab_count;
  EXPECT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(5, tab_count);

  // Alternate focus for the tab.
  const bool kFocusPage[3][5] = {
    { true, true, true, true, false },
    { false, false, false, false, false },
    { false, true, false, true, false }
  };

  for (int i = 1; i < 3; i++) {
    for (int j = 0; j < 5; j++) {
      // Click on the tab.
      ActivateTabByClick(automation(), window.get(), j);

      // Activate the location bar or the page.
      int view_id = kFocusPage[i][j] ? VIEW_ID_TAB_CONTAINER :
                                       VIEW_ID_LOCATION_BAR;

      gfx::Rect bounds;
      EXPECT_TRUE(window->GetViewBounds(view_id, &bounds, true));
      POINT click(bounds.CenterPoint().ToPOINT());
      EXPECT_TRUE(window->SimulateOSClick(click,
          views::Event::EF_LEFT_BUTTON_DOWN));
      ::Sleep(kActionDelayMs);
    }

    // Now come back to the tab and check the right view is focused.
    for (int j = 0; j < 5; j++) {
      // Click on the tab.
      ActivateTabByClick(automation(), window.get(), j);

      // Activate the location bar or the page.
      int exp_view_id = kFocusPage[i][j] ? VIEW_ID_TAB_CONTAINER :
                                           VIEW_ID_LOCATION_BAR;
      int focused_view_id;
      EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
      EXPECT_EQ(exp_view_id, focused_view_id);
    }
  }
}

// Background window does not steal focus.
TEST_F(BrowserFocusTest, BackgroundBrowserDontStealFocus) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to our test page.
  GURL simple_page_url = server->TestServerPageW(kSimplePage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  tab->NavigateToURL(simple_page_url);

  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get() != NULL);
  scoped_ptr<BrowserProxy> browser(
      automation()->GetBrowserForWindow(window.get()));
  ASSERT_TRUE(browser.get() != NULL);

  // Open a new browser window.
  EXPECT_TRUE(automation()->OpenNewBrowserWindow(SW_SHOWNORMAL));
  scoped_ptr<WindowProxy> new_window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get() != NULL);
  scoped_ptr<BrowserProxy> new_browser(
      automation()->GetBrowserForWindow(new_window.get()));
  ASSERT_TRUE(new_browser.get() != NULL);

  GURL steal_focus_url = server->TestServerPageW(kStealFocusPage);
  new_browser->AppendTab(steal_focus_url);

  // Make the first browser active
  EXPECT_TRUE(window->Activate());

  // Wait for the focus to be stolen by the other browser.
  ::Sleep(2000);

  // Make sure the 1st browser window is still active.
  bool is_active = false;
  EXPECT_TRUE(window->IsActive(&is_active));
  EXPECT_TRUE(is_active);
}

// Page cannot steal focus when focus is on location bar.
TEST_F(BrowserFocusTest, LocationBarLockFocus) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());

  // Open the page that steals focus.
  GURL url = server->TestServerPageW(kStealFocusPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  tab->NavigateToURL(url);

  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get() != NULL);
  scoped_ptr<BrowserProxy> browser(
      automation()->GetBrowserForWindow(window.get()));
  ASSERT_TRUE(browser.get() != NULL);

  // Click on the location bar.
  gfx::Rect bounds;
  EXPECT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &bounds, true));
  POINT click(bounds.CenterPoint().ToPOINT());
  EXPECT_TRUE(window->SimulateOSClick(click,
      views::Event::EF_LEFT_BUTTON_DOWN));
  ::Sleep(kActionDelayMs);

  // Wait for the page to steal focus.
  ::Sleep(2000);

  // Make sure the location bar is still focused.
  int focused_view_id;
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);
}

// Focus traversal
TEST_F(BrowserFocusTest, FocusTraversal) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());

  // Open the page the test page.
  GURL url = server->TestServerPageW(kTypicalPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  tab->NavigateToURL(url);

  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get() != NULL);
  scoped_ptr<BrowserProxy> browser(
    automation()->GetBrowserForWindow(window.get()));
  ASSERT_TRUE(browser.get() != NULL);

  // Click on the location bar.
  gfx::Rect bounds;
  EXPECT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &bounds, true));
  POINT click(bounds.CenterPoint().ToPOINT());
  EXPECT_TRUE(window->SimulateOSClick(click,
      views::Event::EF_LEFT_BUTTON_DOWN));
  ::Sleep(kActionDelayMs);

  const wchar_t* kExpElementIDs[] = {
    L"",  // Initially no element in the page should be focused
          // (the location bar is focused).
    L"textEdit", L"searchButton", L"luckyButton", L"googleLink", L"gmailLink",
    L"gmapLink"
  };

  // Test forward focus traversal.
  for (int i = 0; i < 3; ++i) {
    // Location bar should be focused.
    int focused_view_id;
    EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
    EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);

    // Now let's press tab to move the focus.
    for (int j = 0; j < 7; ++j) {
      // Let's make sure the focus is on the expected element in the page.
      std::wstring actual;
      ASSERT_TRUE(tab->ExecuteAndExtractString(L"",
          L"window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[j], actual.c_str());

      window->SimulateOSKeyPress(L'\t', 0);
      ::Sleep(kActionDelayMs);
    }
  }

  // Now let's try reverse focus traversal.
  for (int i = 0; i < 3; ++i) {
    // Location bar should be focused.
    int focused_view_id;
    EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
    EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);

    // Now let's press tab to move the focus.
    for (int j = 0; j < 7; ++j) {
      window->SimulateOSKeyPress(L'\t', views::Event::EF_SHIFT_DOWN);
      ::Sleep(kActionDelayMs);

      // Let's make sure the focus is on the expected element in the page.
      std::wstring actual;
      ASSERT_TRUE(tab->ExecuteAndExtractString(L"",
          L"window.domAutomationController.send(getFocusedElement());",
          &actual));
      ASSERT_STREQ(kExpElementIDs[6 - j], actual.c_str());
    }
  }
}

// Make sure Find box can request focus, even when it is already open.
TEST_F(BrowserFocusTest, FindFocusTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot);
  ASSERT_TRUE(NULL != server.get());

  // Open some page (any page that doesn't steal focus).
  GURL url = server->TestServerPageW(kTypicalPage);
  scoped_ptr<TabProxy> tab(GetActiveTab());
  tab->NavigateToURL(url);

  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  ASSERT_TRUE(window.get() != NULL);
  scoped_ptr<BrowserProxy> browser(
      automation()->GetBrowserForWindow(window.get()));
  ASSERT_TRUE(browser.get() != NULL);

  // Press Ctrl+F, which will make the Find box open and request focus.
  static const int VK_F = 0x46;
  EXPECT_TRUE(window->SimulateOSKeyPress(VK_F, views::Event::EF_CONTROL_DOWN));
  ::Sleep(kActionDelayMs);
  int focused_view_id;
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view_id);

  // Click on the location bar.
  gfx::Rect bounds;
  EXPECT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &bounds, true));
  POINT click(bounds.CenterPoint().ToPOINT());
  EXPECT_TRUE(window->SimulateOSClick(click,
              views::Event::EF_LEFT_BUTTON_DOWN));
  ::Sleep(kActionDelayMs);
  // Make sure the location bar is focused.
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);

  // Now press Ctrl+F again and focus should move to the Find box.
  EXPECT_TRUE(window->SimulateOSKeyPress(VK_F, views::Event::EF_CONTROL_DOWN));
  ::Sleep(kActionDelayMs);
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view_id);

  // Set focus to the page.
  EXPECT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &bounds, true));
  click = bounds.CenterPoint().ToPOINT();
  EXPECT_TRUE(window->SimulateOSClick(click,
                                      views::Event::EF_LEFT_BUTTON_DOWN));
  ::Sleep(kActionDelayMs);
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_TAB_CONTAINER, focused_view_id);

  // Now press Ctrl+F again and focus should move to the Find box.
  EXPECT_TRUE(window->SimulateOSKeyPress(VK_F, views::Event::EF_CONTROL_DOWN));
  ::Sleep(kActionDelayMs);
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, focused_view_id);
}
