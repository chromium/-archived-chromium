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

#include <string>

#include "base/file_util.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/automation/constrained_window_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/views/event.h"
#include "net/base/net_util.h"

class InteractiveConstrainedWindowTest : public UITest {
 protected:
  InteractiveConstrainedWindowTest() {
    show_window_ = true;
  }
};

TEST_F(InteractiveConstrainedWindowTest, TestOpenAndResizeTo) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_ptr<WindowProxy> window(
      automation()->GetWindowForBrowser(browser.get()));
  ASSERT_TRUE(window.get());

  scoped_ptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  std::wstring filename(test_data_directory_);
  file_util::AppendToPath(&filename, L"constrained_files");
  file_util::AppendToPath(&filename,
                          L"constrained_window_onload_resizeto.html");
  ASSERT_TRUE(tab->NavigateToURL(net::FilePathToFileURL(filename)));

  gfx::Rect tab_view_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER,
                                    &tab_view_bounds, true));

  // Simulate a click of the actual link to force user_gesture to be
  // true; if we don't, the resulting popup will be constrained, which
  // isn't what we want to test.
  POINT link_point(tab_view_bounds.CenterPoint().ToPOINT());
  ASSERT_TRUE(window->SimulateOSClick(link_point,
                                      ChromeViews::Event::EF_LEFT_BUTTON_DOWN));

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 1000));

  scoped_ptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  scoped_ptr<WindowProxy> popup_window(
      automation()->GetWindowForBrowser(popup_browser.get()));

  // Make sure we were created with the correct width and height.
  gfx::Rect rect;
  bool is_timeout = false;
  ASSERT_TRUE(popup_window->GetViewBoundsWithTimeout(
                  VIEW_ID_TAB_CONTAINER, &rect, false, 1000, &is_timeout));
  ASSERT_FALSE(is_timeout);
  ASSERT_EQ(rect.width(), 300);
  ASSERT_EQ(rect.height(), 320);

  // Send a click to the popup window to test resizeTo.
  ASSERT_TRUE(popup_window->GetViewBounds(VIEW_ID_TAB_CONTAINER,
                                          &tab_view_bounds, true));
  POINT popup_link_point(tab_view_bounds.CenterPoint().ToPOINT());
  ASSERT_TRUE(popup_window->SimulateOSClick(
                  popup_link_point, ChromeViews::Event::EF_LEFT_BUTTON_DOWN));

  // No idea how to wait here other then sleeping. This timeout used to be lower,
  // then we started hitting it before it was done. :(
  Sleep(5000);

  // The actual content will be LESS than (200, 200) because resizeTo
  // deals with a window's outer{Width,Height} instead of its
  // inner{Width,Height}.
  is_timeout = false;
  ASSERT_TRUE(popup_window->GetViewBoundsWithTimeout(
                  VIEW_ID_TAB_CONTAINER, &rect, false, 1000, &is_timeout));
  ASSERT_FALSE(is_timeout);
  ASSERT_LT(rect.width(), 200);
  ASSERT_LT(rect.height(), 200);
}
