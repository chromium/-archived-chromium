// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>
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
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#include "chromium_strings.h"
#include "generated_resources.h"

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
// TODO(sridharg): Alter, when accessibility objects for Chrome Window,
// Application and Client are corrected.
TEST_F(AccessibilityTest, TestChromeBrowserAccObject) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  ASSERT_TRUE(NULL != hwnd);
  ASSERT_TRUE(NULL != acc_obj);
  CHK_RELEASE(acc_obj);
}

// Check accessibility object for toolbar and its properties Name, Role,
// State. (Add other properties, if their values are fixed all the time.)
TEST_F(AccessibilityTest, TestChromeToolbarAccObject) {
  IAccessible* acc_obj = NULL;
  GetToolbarWnd(&acc_obj);
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
// State. (Add other properties, if their values are fixed all the time.)
TEST_F(AccessibilityTest, TestChromeTabstripAccObject) {
  IAccessible* acc_obj = NULL;
  GetTabStripWnd(&acc_obj);
  ASSERT_TRUE(NULL != acc_obj);

  // Check Name - IDS_ACCNAME_TABSTRIP.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_TABSTRIP), GetName(acc_obj));
  // Check Role - "grouping".
  EXPECT_EQ(ROLE_SYSTEM_GROUPING, GetRole(acc_obj));
  // Check State - "focusable"
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(acc_obj));

  CHK_RELEASE(acc_obj);
}

// Check Browser buttons and their Name, Role, State.
TEST_F(AccessibilityTest, TestChromeButtons) {
  // Get browser accessibility object.
  IAccessible* browser = NULL;
  GetChromeBrowserWnd(&browser);
  ASSERT_TRUE(NULL != browser);

  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  VARIANT button;

  // Check Minimize button and its Name, Role, State.
  hr = GetBrowserMinimizeButton(&acc_obj, &button);
  // Not a complete accessible object, as Minimize is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read properties.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_MINIMIZE),
            GetName(browser, button));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(browser, button));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(browser, button));
  CHK_RELEASE(acc_obj);

  // Check Maximize button and its Name, Role, State.
  GetBrowserMaximizeButton(&acc_obj, &button);
  // Not a complete accessible object, as Maximize is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read properties.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_MAXIMIZE),
            GetName(browser, button));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(browser, button));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(browser, button));
  CHK_RELEASE(acc_obj);

  // Check Restore button and its Name, Role, State.
  GetBrowserRestoreButton(&acc_obj, &button);
  // Not a complete accessible object, as Restore is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read properties.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_RESTORE),
            GetName(browser, button));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(browser, button));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_INVISIBLE,
            GetState(browser, button));
  CHK_RELEASE(acc_obj);

  // Check Close button and its Name, Role, State.
  GetBrowserCloseButton(&acc_obj, &button);
  // Not a complete accessible object, as Close is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read properties.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_CLOSE), GetName(browser, button));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(browser, button));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(browser, button));

  CHK_RELEASE(acc_obj);
  CHK_RELEASE(browser);
}

// Check Star button and its Name, Role, State.
TEST_F(AccessibilityTest, TestStarButton) {
  // Get toolbar accessibility object.
  IAccessible* toolbar = NULL;
  GetToolbarWnd(&toolbar);
  ASSERT_TRUE(NULL != toolbar);

  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  VARIANT button;

  // Check button and its Name, Role, State.
  hr = GetStarButton(&acc_obj, &button);
  // It is not complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read properties.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_STAR), GetName(toolbar, button));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, GetRole(toolbar, button));
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(toolbar, button));

  CHK_RELEASE(acc_obj);
  CHK_RELEASE(toolbar);
}

// Check Star button and its Name, Role, State, upon adding a new tab.
TEST_F(AccessibilityTest, TestStarBtnStatusOnNewTab) {
  // Get toolbar accessibility object.
  IAccessible* toolbar = NULL;
  GetToolbarWnd(&toolbar);
  ASSERT_TRUE(NULL != toolbar);

  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  VARIANT button;

  // Check button and its Name, Role, State.
  hr = GetStarButton(&acc_obj, &button);
  // Not a complete accessible object, as Star is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(toolbar, button));

  // Now, check Star status in different situations.
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());

  // Set URL and check button status.
  scoped_ptr<TabProxy> tab1(window->GetTab(0));
  ASSERT_TRUE(tab1.get());
  std::wstring test_file1 = test_data_directory_;
  file_util::AppendToPath(&test_file1, L"title1.html");
  tab1->NavigateToURL(net::FilePathToFileURL(test_file1));
  Sleep(kWaitForActionMsec);
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(toolbar, button));

  // Add empty new tab and check status.
  int old_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  int new_tab_count;
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count.
  ASSERT_GE(new_tab_count, old_tab_count);
  // Also, check accessibility object's children.
  Sleep(1000);
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(toolbar, button));

  // Add new tab with URL and check status.
  old_tab_count = new_tab_count;
  std::wstring test_file2 = test_data_directory_;
  file_util::AppendToPath(&test_file2, L"title1.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(test_file2)));
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count. Also, check accessibility object's children.
  ASSERT_GE(new_tab_count, old_tab_count);
  Sleep(kWaitForActionMsec);
  EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(toolbar, button));

  CHK_RELEASE(acc_obj);
  CHK_RELEASE(toolbar);
}

// Check Back button and its Name, Role, State.
TEST_F(AccessibilityTest, TestBackButton) {
  // Get toolbar accessibility object.
  IAccessible* toolbar = NULL;
  GetToolbarWnd(&toolbar);
  ASSERT_TRUE(NULL != toolbar);

  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  VARIANT button;

  // Check button and its Name, Role, State.
  hr = GetBackButton(&acc_obj, &button);
  // Not a complete accessible object, as Back is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);

  // Read properties.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_BACK),
            GetName(toolbar, button));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(toolbar, button));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  CHK_RELEASE(acc_obj);
  CHK_RELEASE(toolbar);
}

// Check Back button and its Name, Role, State.
// This test is disabled. See bug 1119183.
TEST_F(AccessibilityTest, DISABLED_TestBackBtnStatusOnNewTab) {
  // Get toolbar accessibility object.
  IAccessible* toolbar = NULL;
  GetToolbarWnd(&toolbar);
  ASSERT_TRUE(NULL != toolbar);

  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  VARIANT button;

  // Check button and its Name, Role, State.
  hr = GetBackButton(&acc_obj, &button);
  // Not a complete accessible object, as Back is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  // Now check Back status in different situations.
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  int old_tab_count = -1;
  int new_tab_count = -1;

  // Set URL and check button status.
  scoped_ptr<TabProxy> tab1(window->GetTab(0));
  ASSERT_TRUE(tab1.get());
  std::wstring test_file1 = test_data_directory_;
  file_util::AppendToPath(&test_file1, L"title1.html");
  tab1->NavigateToURL(net::FilePathToFileURL(test_file1));
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP | STATE_SYSTEM_FOCUSABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(toolbar, button));
  }
  // Go Back and check status.
  window->ApplyAccelerator(IDC_BACK);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  // Add empty new tab and check status.
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count. Also, check accessibility object's children.
  ASSERT_GE(new_tab_count, old_tab_count);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  // Add new tab with URL and check status.
  old_tab_count = new_tab_count;
  std::wstring test_file2 = test_data_directory_;
  file_util::AppendToPath(&test_file2, L"title1.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(test_file2)));
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count. Also, check accessibility object's children.
  ASSERT_GE(new_tab_count, old_tab_count);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  CHK_RELEASE(acc_obj);
  CHK_RELEASE(toolbar);
}

// Check Forward button and its Name, Role, State.
TEST_F(AccessibilityTest, TestForwardButton) {
  // Get toolbar accessibility object.
  IAccessible* toolbar = NULL;
  GetToolbarWnd(&toolbar);
  ASSERT_TRUE(NULL != toolbar);

  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  VARIANT button;

  // Check button and its Name, Role, State.
  hr = GetForwardButton(&acc_obj, &button);
  // Not a complete accessible object, as Forward is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read properties.
  EXPECT_EQ(l10n_util::GetString(IDS_ACCNAME_FORWARD),
            GetName(toolbar, button));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONDROPDOWN, GetRole(toolbar, button));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  CHK_RELEASE(acc_obj);
  CHK_RELEASE(toolbar);
}

// Check Back button and its Name, Role, State.
// This test is disabled. See bug 1119183.
TEST_F(AccessibilityTest, DISABLED_TestForwardBtnStatusOnNewTab) {
  // Get toolbar accessibility object.
  IAccessible* toolbar = NULL;
  GetToolbarWnd(&toolbar);
  ASSERT_TRUE(NULL != toolbar);

  HRESULT hr = S_OK;
  IAccessible* acc_obj = NULL;
  VARIANT button;

  // Check button and its Name, Role, State.
  hr = GetForwardButton(&acc_obj, &button);
  // Not a complete accessible object, as Forward is an element leaf.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == acc_obj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  // Now check Back status in different situations.
  scoped_ptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  int old_tab_count = -1;
  int new_tab_count = -1;

  // Set URL and check button status.
  scoped_ptr<TabProxy> tab1(window->GetTab(0));
  ASSERT_TRUE(tab1.get());
  std::wstring test_file1 = test_data_directory_;
  file_util::AppendToPath(&test_file1, L"title1.html");
  tab1->NavigateToURL(net::FilePathToFileURL(test_file1));
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }
  // Go Back and check status.
  window->ApplyAccelerator(IDC_BACK);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP | STATE_SYSTEM_FOCUSABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE, GetState(toolbar, button));
  }
  // Go Forward and check status.
  window->ApplyAccelerator(IDC_FORWARD);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  // Add empty new tab and check status.
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEW_TAB));
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count.
  ASSERT_GE(new_tab_count, old_tab_count);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  // Add new tab with URL and check status.
  old_tab_count = new_tab_count;
  std::wstring test_file2 = test_data_directory_;
  file_util::AppendToPath(&test_file2, L"title1.html");
  ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(test_file2)));
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count.
  ASSERT_GE(new_tab_count, old_tab_count);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(STATE_SYSTEM_HASPOPUP |
              STATE_SYSTEM_FOCUSABLE |
              STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  } else {
    EXPECT_EQ(STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_UNAVAILABLE,
              GetState(toolbar, button));
  }

  CHK_RELEASE(acc_obj);
  CHK_RELEASE(toolbar);
}

