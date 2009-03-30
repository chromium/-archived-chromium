// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>

#include "base/file_util.h"
#include "base/win_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/l10n_util.h"
#include "chrome/test/accessibility/accessibility_util.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

namespace {

#define CHK_RELEASE(obj) { if (obj) { (obj)->Release(); (obj) = NULL; } }

class AccessibilityTest : public UITest {
 protected:
  AccessibilityTest() {
    show_window_ = true;
    CoInitialize(NULL);
  }
  ~AccessibilityTest() {
    CoUninitialize();
  }
};
}  // Namespace.

// Check browser handle and accessibility object browser client.
TEST_F(AccessibilityTest, DISABLED_TestChromeBrowserAccObject) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);

  ASSERT_TRUE(NULL != hwnd);
  ASSERT_TRUE(NULL != acc_obj);

  CHK_RELEASE(acc_obj);
}

// Check accessibility object for toolbar and its properties Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestChromeToolbarAccObject) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  hr = GetToolbarAccessible(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check Name - IDS_ACCNAME_TOOLBAR.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_TOOLBAR), GetName(acc_obj));
  // Check Role - "tool bar".
  EXPECT_EQ(ROLE_SYSTEM_TOOLBAR, GetRole(acc_obj));
  // Check State - "focusable"
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  CHK_RELEASE(acc_obj);
}

// Check accessibility object for tabstrip and its properties Name, Role,
// State.
TEST_F(AccessibilityTest, DISABLED_TestChromeTabstripAccObject) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  hr = GetTabStripAccessible(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check Name - IDS_ACCNAME_TABSTRIP.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_TABSTRIP), GetName(acc_obj));
  // Check Role - "grouping".
  EXPECT_EQ(ROLE_SYSTEM_GROUPING, GetRole(acc_obj));
  // Check State - "focusable"
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  CHK_RELEASE(acc_obj);
}

// This test is disabled for now, see issue 2243.
// Check Browser buttons and their Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestChromeButtons) {
  // TODO(klink): Implement with indexing from ViewIDs.
}

// Check Back button and its Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestBackButton) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Back button.
  hr = GetBackButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_BACK), GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(acc_obj));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(acc_obj));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(acc_obj));
  }

  CHK_RELEASE(acc_obj);
}

// Check Back button and its Name, Role, State, upon adding a new tab.
TEST_F(AccessibilityTest, DISABLED_TestBackBtnStatusOnNewTab) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Back button.
  hr = GetBackButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_BACK), GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(acc_obj));
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));

  // Now check Back status in different situations.
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  int tab_count = -1;

  // Set URL and check button status.
  scoped_ptr<TabProxy> tab1(window->GetTab(0));
  ASSERT_TRUE(tab1.get());
  std::wstring test_file1 = test_data_directory_;
  file_util::AppendToPath(&test_file1, L"title1.html");
  tab1->NavigateToURL(net::FilePathToFileURL(test_file1));
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP | STATE_SYSTEM_FOCUSABLE,
            GetState(acc_obj));
  // Go Back and check status.
  window->ApplyAccelerator(IDC_BACK);
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));

  // Add empty new tab and check status.
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  ASSERT_TRUE(window->WaitForTabCountToBecome(tab_count + 1,
                                              action_max_timeout_ms()));
  // Check accessibility object's children.
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));

  // Add new tab with URL and check status.
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  std::wstring test_file2 = test_data_directory_;
  file_util::AppendToPath(&test_file2, L"title1.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(test_file2)));
  ASSERT_TRUE(window->WaitForTabCountToBecome(tab_count + 1,
                                              action_max_timeout_ms()));
  // Check accessibility object's children.
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));
  CHK_RELEASE(acc_obj);
}

// Check Forward button and its Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestForwardButton) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Forward button.
  hr = GetForwardButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_FORWARD),
            GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(acc_obj));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(acc_obj));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(acc_obj));
  }

  CHK_RELEASE(acc_obj);
}

// Check Forward button and its Name, Role, State, upon adding a new tab.
TEST_F(AccessibilityTest, DISABLED_TestForwardBtnStatusOnNewTab) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Forward button.
  hr = GetForwardButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_FORWARD),
            GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(acc_obj));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(acc_obj));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(acc_obj));
  }

  // Now check Back status in different situations.
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  int tab_count = -1;

  // Set URL and check button status.
  scoped_ptr<TabProxy> tab1(window->GetTab(0));
  ASSERT_TRUE(tab1.get());
  std::wstring test_file1 = test_data_directory_;
  file_util::AppendToPath(&test_file1, L"title1.html");
  tab1->NavigateToURL(net::FilePathToFileURL(test_file1));
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));
  // Go Back and check status.
  window->ApplyAccelerator(IDC_BACK);
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP | STATE_SYSTEM_FOCUSABLE,
            GetState(acc_obj));
  // Go Forward and check status.
  window->ApplyAccelerator(IDC_FORWARD);
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));

  // Add empty new tab and check status.
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  ASSERT_TRUE(window->WaitForTabCountToBecome(tab_count + 1,
                                              action_max_timeout_ms()));
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));

  // Add new tab with URL and check status.
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  std::wstring test_file2 = test_data_directory_;
  file_util::AppendToPath(&test_file2, L"title1.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(test_file2)));
  ASSERT_TRUE(window->WaitForTabCountToBecome(tab_count + 1,
                                              action_max_timeout_ms()));
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
            STATE_SYSTEM_FOCUSABLE |
            STATE_SYSTEM_UNAVAILABLE,
            GetState(acc_obj));
  CHK_RELEASE(acc_obj);
}

// Check Star button and its Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestStarButton) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Star button.
  hr = GetStarButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_STAR), GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(acc_obj));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  CHK_RELEASE(acc_obj);
}

// Check Star button and its Name, Role, State, upon adding a new tab.
TEST_F(AccessibilityTest, DISABLED_TestStarBtnStatusOnNewTab) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Star button.
  hr = GetStarButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_STAR), GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(acc_obj));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  // Now, check Star status in different situations.
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  // Set URL and check button status.
  scoped_ptr<TabProxy> tab1(window->GetTab(0));
  ASSERT_TRUE(tab1.get());
  std::wstring test_file1 = test_data_directory_;
  file_util::AppendToPath(&test_file1, L"title1.html");
  tab1->NavigateToURL(net::FilePathToFileURL(test_file1));
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  // Add empty new tab and check status.
  int tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  ASSERT_TRUE(window->WaitForTabCountToBecome(tab_count + 1,
                                              action_max_timeout_ms()));
  // Check tab count. Also, check accessibility object's state.
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  // Add new tab with URL and check status.
  ASSERT_TRUE(window->GetTabCount(&tab_count));
  std::wstring test_file2 = test_data_directory_;
  file_util::AppendToPath(&test_file2, L"title1.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(test_file2)));
  ASSERT_TRUE(window->WaitForTabCountToBecome(tab_count + 1,
                                              action_max_timeout_ms()));
  // Check tab count. Also, check accessibility object's state.
  Sleep(sleep_timeout_ms());
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  CHK_RELEASE(acc_obj);
}

// Check Go button and its Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestGoButton) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Go button.
  hr = GetGoButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_GO), GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(acc_obj));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  CHK_RELEASE(acc_obj);
}

// Check Page menu button and its Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestPageMenuButton) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for Page menu button.
  hr = GetPageMenuButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_PAGE), GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(acc_obj));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP | STATE_SYSTEM_FOCUSABLE,
              GetState(acc_obj));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));
  }

  CHK_RELEASE(acc_obj);
}

// Check App (wrench) menu button and its Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestAppMenuButton) {
  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;

  // Retrieve IAccessible for App menu button.
  hr = GetAppMenuButton(&acc_obj);
  ASSERT_TRUE(S_OK == hr);
  ASSERT_TRUE(NULL != acc_obj);

  // Check button and its Name, Role, State.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_APP), GetName(acc_obj));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(acc_obj));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP | STATE_SYSTEM_FOCUSABLE,
              GetState(acc_obj));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));
  }

  CHK_RELEASE(acc_obj);
}
