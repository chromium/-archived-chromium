// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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
                                       ChromeViews::Event::EF_LEFT_BUTTON_DOWN))
    return false;

  // Wait a bit to let the click be processed.
  ::Sleep(kActionDelayMs);

  return true;
}

}  // namespace

TEST_F(FindInPageTest, CrashEscHandlers) {
  TestServer server(kDocRoot);

  scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
  EXPECT_TRUE(window.get() != NULL);

  scoped_ptr<BrowserProxy> browser(automation()->
      GetBrowserForWindow(window.get()));

  // First we navigate to our test page (tab A).
  GURL url = server.TestServerPageW(kSimplePage);
  scoped_ptr<TabProxy> tabA(GetActiveTab());
  EXPECT_NE(AUTOMATION_MSG_NAVIGATION_ERROR, tabA->NavigateToURL(url));

  EXPECT_TRUE(tabA->OpenFindInPage());

  // Open another tab (tab B).
  EXPECT_TRUE(browser->AppendTab(url));
  scoped_ptr<TabProxy> tabB(GetActiveTab());

  EXPECT_TRUE(tabB->OpenFindInPage());

  // Select tab A.
  EXPECT_TRUE(ActivateTabByClick(automation(), window.get(), 0));

  // Close tab B.
  EXPECT_TRUE(tabB->Close(true));

  // Click on the location bar so that Find box loses focus.
  gfx::Rect bounds;
  EXPECT_TRUE(window->GetViewBounds(VIEW_ID_LOCATION_BAR, &bounds, false));
  POINT click(bounds.CenterPoint().ToPOINT());
  EXPECT_TRUE(window->SimulateOSClick(click,
      ChromeViews::Event::EF_LEFT_BUTTON_DOWN));
  ::Sleep(kActionDelayMs);
  int focused_view_id;
  EXPECT_TRUE(window->GetFocusedViewID(&focused_view_id));
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, focused_view_id);

  // This used to crash until bug 1303709 was fixed.
  EXPECT_TRUE(window->SimulateOSKeyPress(VK_ESCAPE, 0));
  ::Sleep(kActionDelayMs);
}
