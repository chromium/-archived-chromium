// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "views/event.h"

class BlockedPopupContainerInteractiveTest : public UITest {
 protected:
  BlockedPopupContainerInteractiveTest() {
    show_window_ = true;
  }

  virtual void SetUp() {
    UITest::SetUp();

    browser_ = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(browser_.get());

    window_ = browser_->GetWindow();
    ASSERT_TRUE(window_.get());

    tab_ = browser_->GetTab(0);
    ASSERT_TRUE(tab_.get());
  }

  void NavigateMainTabTo(const std::wstring& file_name) {
    FilePath filename(test_data_directory_);
    filename = filename.AppendASCII("constrained_files");
    filename = filename.Append(FilePath::FromWStringHack(file_name));
    ASSERT_TRUE(tab_->NavigateToURL(net::FilePathToFileURL(filename)));
  }

  void SimulateClickInCenterOf(const scoped_refptr<WindowProxy>& window) {
    gfx::Rect tab_view_bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER,
                                      &tab_view_bounds, true));

    // Simulate a click of the actual link to force user_gesture to be
    // true; if we don't, the resulting popup will be constrained, which
    // isn't what we want to test.
    POINT link_point(tab_view_bounds.CenterPoint().ToPOINT());
    ASSERT_TRUE(window->SimulateOSClick(link_point,
                                        views::Event::EF_LEFT_BUTTON_DOWN));
  }

  scoped_refptr<BrowserProxy> browser_;
  scoped_refptr<WindowProxy> window_;
  scoped_refptr<TabProxy> tab_;
};

TEST_F(BlockedPopupContainerInteractiveTest, TestOpenAndResizeTo) {
  NavigateMainTabTo(L"constrained_window_onload_resizeto.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 1000));

  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser != NULL);
  scoped_refptr<WindowProxy> popup_window(popup_browser->GetWindow());
  ASSERT_TRUE(popup_window != NULL);

  // Make sure we were created with the correct width and height.
  gfx::Rect rect;
  bool is_timeout = false;
  ASSERT_TRUE(popup_window->GetViewBoundsWithTimeout(
                  VIEW_ID_TAB_CONTAINER, &rect, false, 1000, &is_timeout));
  ASSERT_FALSE(is_timeout);
  ASSERT_EQ(300, rect.width());
  ASSERT_EQ(320, rect.height());

  SimulateClickInCenterOf(popup_window);

  // No idea how to wait here other then sleeping. This timeout used to be
  // lower, then we started hitting it before it was done. :(
  PlatformThread::Sleep(5000);

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

// Helper function used to get the number of blocked popups out of the window
// title.
bool ParseCountOutOfTitle(const std::wstring& title, int* output) {
  // Since we will be reading the number of popup windows open by grabbing the
  // number out of the window title, and that format string is localized, we
  // need to find out the offset into that string.
  const wchar_t* placeholder = L"XXXX";
  size_t offset =
      l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT, placeholder).
      find(placeholder);

  std::wstring number;
  while (offset < title.size() && iswdigit(title[offset])) {
    number += title[offset];
    offset++;
  }

  return StringToInt(number, output);
}

// Tests that in the window.open() equivalent of a fork bomb, we stop building
// windows.
TEST_F(BlockedPopupContainerInteractiveTest, DontSpawnEndlessPopups) {
  NavigateMainTabTo(L"infinite_popups.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 1000));

  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser.get());
  scoped_refptr<TabProxy> popup_tab(popup_browser->GetTab(0));
  ASSERT_TRUE(popup_tab.get());

  // And now we spin, waiting to make sure that we don't spawn popup
  // windows endlessly. The current limit is 25, so allowing for possible race
  // conditions and one off errors, don't break out until we go over 30 popup
  // windows (in which case we are bork bork bork).
  const int kMaxPopupWindows = 30;

  int popup_window_count = 0;
  int new_popup_window_count = 0;
  int times_slept = 0;
  bool continuing = true;
  while (continuing && popup_window_count < kMaxPopupWindows) {
    ASSERT_TRUE(popup_tab->GetBlockedPopupCount(&new_popup_window_count));
    if (new_popup_window_count == popup_window_count) {
      if (times_slept == 10) {
        continuing = false;
      } else {
        // Nothing intereseting is going on wait it out.
        PlatformThread::Sleep(automation::kSleepTime);
        times_slept++;
      }
    } else {
      times_slept = 0;
    }

    EXPECT_GE(new_popup_window_count, popup_window_count);
    EXPECT_LE(new_popup_window_count, kMaxPopupWindows);
    popup_window_count = new_popup_window_count;
  }
}

// Make sure that we refuse to close windows when a constrained popup is
// displayed.
TEST_F(BlockedPopupContainerInteractiveTest, WindowOpenWindowClosePopup) {
  NavigateMainTabTo(L"openclose_main.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 5000));

  // Make sure we have a blocked popup notification
  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser.get());
  scoped_refptr<WindowProxy> popup_window(popup_browser->GetWindow());
  ASSERT_TRUE(popup_window.get());
  scoped_refptr<TabProxy> popup_tab(popup_browser->GetTab(0));
  ASSERT_TRUE(popup_tab.get());
  ASSERT_TRUE(popup_tab->WaitForBlockedPopupCountToChangeTo(1, 1000));

  // Ensure we didn't close the first popup window.
  ASSERT_FALSE(automation()->WaitForWindowCountToBecome(1, 3000));
}

TEST_F(BlockedPopupContainerInteractiveTest, BlockAlertFromBlockedPopup) {
  NavigateMainTabTo(L"block_alert.html");

  // Wait for there to be an app modal dialog (and fail if it's shown).
  ASSERT_FALSE(automation()->WaitForAppModalDialog(4000));

  // Ensure one browser window.
  int browser_window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&browser_window_count));
  ASSERT_EQ(1, browser_window_count);

  // Ensure one blocked popup window: the popup didn't escape.
  int popup_count = 0;
  ASSERT_TRUE(tab_->GetBlockedPopupCount(&popup_count));
  ASSERT_EQ(1, popup_count);
}

TEST_F(BlockedPopupContainerInteractiveTest, ShowAlertFromNormalPopup) {
  NavigateMainTabTo(L"show_alert.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 5000));

  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser.get());
  scoped_refptr<WindowProxy> popup_window(popup_browser->GetWindow());
  ASSERT_TRUE(popup_window.get());
  scoped_refptr<TabProxy> popup_tab(popup_browser->GetTab(0));
  ASSERT_TRUE(popup_tab.get());

  SimulateClickInCenterOf(popup_window);

  // Wait for there to be an app modal dialog.
  ASSERT_TRUE(automation()->WaitForAppModalDialog(5000));
}

// Make sure that window focus works while creating a popup window so that we
// don't
TEST_F(BlockedPopupContainerInteractiveTest, DontBreakOnBlur) {
  NavigateMainTabTo(L"window_blur_test.html");
  SimulateClickInCenterOf(window_);

  // Wait for the popup window to open.
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 1000));

  // We popup shouldn't be closed by the onblur handler.
  ASSERT_FALSE(automation()->WaitForWindowCountToBecome(1, 1500));
}
