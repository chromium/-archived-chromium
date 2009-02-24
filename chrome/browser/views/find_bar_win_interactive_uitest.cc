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
static const int kActionDelayMs = 500;
static const wchar_t kDocRoot[] = L"chrome/test/data";
static const wchar_t kSimplePage[] = L"404_is_enough_for_us.html";

class FindInPageTest : public UITest {
 public:
  FindInPageTest() {
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

TEST_F(FindInPageTest, CrashEscHandlers) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  ASSERT_TRUE(browser.get() != NULL);
  scoped_ptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get() != NULL);

  // First we navigate to our test page (tab A).
  GURL url = server->TestServerPageW(kSimplePage);
  scoped_ptr<TabProxy> tabA(GetActiveTab());
  EXPECT_NE(AUTOMATION_MSG_NAVIGATION_ERROR, tabA->NavigateToURL(url));

  EXPECT_TRUE(browser->OpenFindInPage());

  // Open another tab (tab B).
  EXPECT_TRUE(browser->AppendTab(url));
  scoped_ptr<TabProxy> tabB(GetActiveTab());

  EXPECT_TRUE(browser->OpenFindInPage());

  // Select tab A.
  EXPECT_TRUE(ActivateTabByClick(automation(), window.get(), 0));

  // Close tab B.
  EXPECT_TRUE(tabB->Close(true));

  // Click on the location bar so that Find box loses focus.
  gfx::Rect bounds;
  EXPECT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &bounds, false));
  POINT click(bounds.CenterPoint().ToPOINT());
  EXPECT_TRUE(window->SimulateOSClick(click,
                                      views::Event::EF_LEFT_BUTTON_DOWN));
  ::Sleep(kActionDelayMs);
  int focused_view_id;
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);

  // This used to crash until bug 1303709 was fixed.
  EXPECT_TRUE(window->SimulateOSKeyPress(VK_ESCAPE, 0));
  ::Sleep(kActionDelayMs);
}
