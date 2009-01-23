// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests browsing in app-mode.  Specifically, insures that navigation to
// new windows launch in the default protocol handler and that navigations
// within the same frame do not.  Outside of app-mode these tests are covered
// by normal navigation and the fork test.

#include "base/string_util.h"
#include "base/file_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";
const wchar_t kTestFileName[] = L"appmodenavigation_test.html";

class AppModeNavigationTest : public UITest {
 protected:
  AppModeNavigationTest() : UITest() {
    show_window_ = true;

    // Launch chrome in app mode.
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, kTestFileName);
    std::wstring url = ASCIIToWide(net::FilePathToFileURL(test_file).spec());
    launch_arguments_.AppendSwitchWithValue(switches::kApp, url);
  }

  // Helper function for evaluation.
  HWND GetMainBrowserWindow() {
    scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    scoped_ptr<WindowProxy> window(browser->GetWindow());

    HWND window_handle;
    EXPECT_TRUE(window->GetHWND(&window_handle));
    return window_handle;
  }

  // Get the window title from the main window.
  std::wstring GetMainBrowserWindowTitle() {
    HWND window_handle = GetMainBrowserWindow();
    std::wstring result;
    int length = ::GetWindowTextLength(window_handle) + 1;
    ::GetWindowText(window_handle, WriteInto(&result, length), length);
    return result;
  }

  // Given a page title, returns the expected window caption string.
  std::wstring WindowCaptionFromPageTitle(const std::wstring& page_title) {
    if (page_title.empty())
      return l10n_util::GetString(IDS_PRODUCT_NAME);
    return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, page_title);
  }

  // Selects the anchor tag by index and navigates to it.  This is necssary
  // since the automation API's navigate the window by simulating address bar
  // entries.
  void NavigateToIndex(int index) {
    scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
    ASSERT_TRUE(window->Activate());
    Sleep(action_timeout_ms());
    // We are in app mode, hence tab moves from focus to the next UI element
    // in the view.
    for (; index > 0; --index) {
      window->SimulateOSKeyPress(L'\t', 0);  // 0 signifies no modifier.
    }
    Sleep(action_timeout_ms());
    window->SimulateOSKeyPress(L'\r', 0);
    Sleep(action_timeout_ms());
  }

  // Validates that the foreground window is the external protocol handler
  // validation window, and if so dismisses it by clicking on the default
  // input.
  bool DismissExternalLauncherPopup() {
    // The currently active window should be the protocol handler if this is
    // and external link.
    scoped_ptr<WindowProxy> window(automation()->GetActiveWindow());
    HWND hwnd;
    if (!window->GetHWND(&hwnd))
      return false;

    int size = ::GetWindowTextLengthW(hwnd);
    scoped_ptr<wchar_t> buffer(new wchar_t[size+1]);
    if (NULL == buffer.get())
      return false;
    ::GetWindowText(hwnd, buffer.get(), size+1);
    if (0 != wcscmp(L"External Protocol Request", buffer.get()))
      return false;

    // The default UI element is the |Cancel| button.
    window->SimulateOSKeyPress(L'\r', 0);
    Sleep(action_timeout_ms());

    return true;
  }
};


// Follow an normal anchor tag with target=blank (external tab).
TEST_F(AppModeNavigationTest, NavigateToNewTab) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab(browser->GetActiveTab());

  int orig_tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_TRUE(orig_process_count >= 1);

  NavigateToIndex(1);  // First link test.

  ASSERT_TRUE(DismissExternalLauncherPopup());

  // Insure all the other processes have terminated.
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());

  // In app-mode there is a single tab.
  int new_tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count, new_tab_count);

  // We must not have navigated.
  EXPECT_EQ(WindowCaptionFromPageTitle(kTestFileName),
            GetMainBrowserWindowTitle());
  EXPECT_EQ(kTestFileName, GetActiveTabTitle());
}

// Open a new tab with a redirect as Gmail does.
TEST_F(AppModeNavigationTest, NavigateByForkToNewTabTest) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab(browser->GetActiveTab());

  int orig_tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_TRUE(orig_process_count >= 1);

  NavigateToIndex(2);  // Second link test.

  ASSERT_TRUE(DismissExternalLauncherPopup());

  // Insure all the other processes have terminated.
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());

  // In app-mode there is a single tab.
  int new_tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count, new_tab_count);

  // We must not have navigated.
  EXPECT_EQ(WindowCaptionFromPageTitle(kTestFileName),
            GetMainBrowserWindowTitle());
  EXPECT_EQ(kTestFileName, GetActiveTabTitle());
}

// Normal navigation
TEST_F(AppModeNavigationTest, NavigateToAboutBlankByLink) {
  scoped_ptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  scoped_ptr<TabProxy> tab(browser->GetActiveTab());

  int orig_tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_TRUE(orig_process_count >= 1);

  NavigateToIndex(3);  // Third link test.

  // Insure all the other processes have terminated.
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());

  // In app-mode there is a single tab.
  int new_tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count, new_tab_count);

  // We must not have navigated.
  EXPECT_EQ(WindowCaptionFromPageTitle(L"about:blank"),
            GetMainBrowserWindowTitle());
  EXPECT_EQ(L"", GetActiveTabTitle());
}

}  // namespace
