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

#include <Objbase.h>
#include <Oleacc.h>
#include "chrome/test/accessibility/accessibility_util.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/app/chrome_dll_resource.h"
#include "base/win_util.h"
#include "base/file_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"


namespace {

#define CHK_RELEASE(obj)  { if (obj) { (obj)->Release(); (obj) = NULL; } }

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
// TODO(sridharg): Alter, when accessibility objects for Chrome Window, Application and
// Client are corrected.
TEST_F(AccessibilityTest, TestChromeBrowserAccObject) {
  IAccessible* p_accobj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&p_accobj);
  ASSERT_TRUE(NULL != hwnd);
  ASSERT_TRUE(NULL != p_accobj);
  CHK_RELEASE(p_accobj);
}

// Check accessibility object for toolbar and it's properties Name, Role, State.
// (Add other properties, if their values are fixed all the time.)
TEST_F(AccessibilityTest, TestChromeToolbarAccObject) {
  IAccessible* p_accobj = NULL;
  GetToolbarWnd(&p_accobj);
  ASSERT_TRUE(NULL != p_accobj);

  // Check Name - "Google Chrome Toolbar".
  EXPECT_EQ(L"Google Chrome Toolbar", GetName(p_accobj));
  // Check Role - "tool bar".
  EXPECT_EQ(L"tool bar", GetRole(p_accobj));
  // Check State - "focusable"
  EXPECT_EQ(L"focusable", GetState(p_accobj));

  CHK_RELEASE(p_accobj);
}

// Check accessibility object for tabstrip and it's properties Name, Role,
// State. (Add other properties, if their values are fixed all the time.)
TEST_F(AccessibilityTest, TestChromeTabstripAccObject) {
  IAccessible* p_accobj = NULL;
  GetTabStripWnd(&p_accobj);
  ASSERT_TRUE(NULL != p_accobj);

  // Check Name - "Tabstrip".
  EXPECT_EQ(L"Tabstrip", GetName(p_accobj));
  // Check Role - "grouping".
  EXPECT_EQ(L"grouping", GetRole(p_accobj));
  // Check State - "focusable"
  EXPECT_EQ(L"focusable", GetState(p_accobj));

  CHK_RELEASE(p_accobj);
}

// Check Browser buttons and their Name, Role, State.
TEST_F(AccessibilityTest, DISABLED_TestChromeButtons) {
  HRESULT      hr       = S_OK;
  IAccessible* p_accobj = NULL;
  VARIANT      button;

  // Get browser accessibility object.
  IAccessible* p_browser = NULL;
  GetChromeBrowserWnd(&p_browser);
  ASSERT_TRUE(NULL != p_browser);

  // Check Minimize button and it's Name, Role, State.
  hr = GetBrowserMinimizeButton(&p_accobj, &button);
  // It is not complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read it's properties.
  EXPECT_EQ(L"Minimize", GetName(p_browser, button));
  EXPECT_EQ(L"push button", GetRole(p_browser, button));
  EXPECT_EQ(L"focusable", GetState(p_browser, button));
  CHK_RELEASE(p_accobj);

  // Check Maximize button and it's Name, Role, State.
  GetBrowserMaximizeButton(&p_accobj, &button);
  // It is an element and not complete accessible object.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read it's properties.
  EXPECT_EQ(L"Maximize", GetName(p_browser, button));
  EXPECT_EQ(L"push button", GetRole(p_browser, button));
  EXPECT_EQ(L"focusable", GetState(p_browser, button));
  CHK_RELEASE(p_accobj);

  // Check Restore button and it's Name, Role, State.
  GetBrowserRestoreButton(&p_accobj, &button);
  // It is an element and not complete accessible object.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read it's properties.
  EXPECT_EQ(L"Restore", GetName(p_browser, button));
  EXPECT_EQ(L"push button", GetRole(p_browser, button));
  EXPECT_EQ(L"focusable, invisible", GetState(p_browser, button));
  CHK_RELEASE(p_accobj);

  // Check Close button and it's Name, Role, State.
  GetBrowserCloseButton(&p_accobj, &button);
  // It is an element and not complete accessible object.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read it's properties.
  EXPECT_EQ(L"Close", GetName(p_browser, button));
  EXPECT_EQ(L"push button", GetRole(p_browser, button));
  EXPECT_EQ(L"focusable", GetState(p_browser, button));
  CHK_RELEASE(p_accobj);

  CHK_RELEASE(p_browser);
}

// Check Star button and their Name, Role, State.
TEST_F(AccessibilityTest, TestStarButton) {
  HRESULT      hr       = S_OK;
  IAccessible* p_accobj = NULL;
  VARIANT      button;

  // Get toolbar accessibility object.
  IAccessible* p_toolbar = NULL;
  GetToolbarWnd(&p_toolbar);
  ASSERT_TRUE(NULL != p_toolbar);

  // Check button and it's Name, Role, State.
  hr = GetStarButton(&p_accobj, &button);
  // It is not complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read it's properties.
  EXPECT_EQ(L"Bookmark", GetName(p_toolbar, button));
  EXPECT_EQ(L"push button", GetRole(p_toolbar, button));
  EXPECT_EQ(L"focusable", GetState(p_toolbar, button));
  CHK_RELEASE(p_accobj);

  CHK_RELEASE(p_toolbar);
}

// Check Star button and their Name, Role, State.
TEST_F(AccessibilityTest, TestStarBtnStatusOnNewTab) {
  HRESULT      hr       = S_OK;
  IAccessible* p_accobj = NULL;
  VARIANT      button;

  // Get toolbar accessibility object.
  IAccessible* p_toolbar = NULL;
  GetToolbarWnd(&p_toolbar);
  ASSERT_TRUE(NULL != p_toolbar);

  // Check button and it's Name, Role, State.
  hr = GetStarButton(&p_accobj, &button);
  // It is not a complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  EXPECT_EQ(L"focusable", GetState(p_toolbar, button));

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
  EXPECT_EQ(L"focusable", GetState(p_toolbar, button));

  // Add empty new tab and check status.
  int old_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEWTAB));
  int new_tab_count;
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count.
  ASSERT_GE(new_tab_count, old_tab_count);
  // Also, check accessibility object's children.
  Sleep(1000);
  EXPECT_EQ(L"focusable", GetState(p_toolbar, button)); // ???

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
  EXPECT_EQ(L"focusable", GetState(p_toolbar, button));

  CHK_RELEASE(p_toolbar);
}

// Check Back button and their Name, Role, State.
TEST_F(AccessibilityTest, TestBackButton) {
  HRESULT      hr       = S_OK;
  IAccessible* p_accobj = NULL;
  VARIANT      button;

  // Get toolbar accessibility object.
  IAccessible* p_toolbar = NULL;
  GetToolbarWnd(&p_toolbar);
  ASSERT_TRUE(NULL != p_toolbar);

  // Check button and it's Name, Role, State.
  hr = GetBackButton(&p_accobj, &button);
  // It is not a complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);

  // Read it's properties.
  EXPECT_EQ(L"Back", GetName(p_toolbar, button));
  EXPECT_EQ(L"drop down button", GetRole(p_toolbar, button));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
  }
  CHK_RELEASE(p_accobj);

  CHK_RELEASE(p_toolbar);
}

// Check Back button and their Name, Role, State.
// This test is disabled. See bug 1119183.
TEST_F(AccessibilityTest, DISABLED_TestBackBtnStatusOnNewTab) {
  HRESULT      hr       = S_OK;
  IAccessible* p_accobj = NULL;
  VARIANT      button;

  // Get toolbar accessibility object.
  IAccessible* p_toolbar = NULL;
  GetToolbarWnd(&p_toolbar);
  ASSERT_TRUE(NULL != p_toolbar);

  // Check button and it's Name, Role, State.
  hr = GetBackButton(&p_accobj, &button);
  // It is not complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
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
    EXPECT_EQ(L"has popup, focusable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable", GetState(p_toolbar, button));
  }
  // Go Back and check status.
  window->ApplyAccelerator(IDC_BACK);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
  }

  // Add empty new tab and check status.
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEWTAB));
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count. Also, check accessibility object's children.
  ASSERT_GE(new_tab_count, old_tab_count);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
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
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
  }

  CHK_RELEASE(p_toolbar);
}

// Check Forward button and their Name, Role, State.
TEST_F(AccessibilityTest, TestForwardButton) {
  HRESULT      hr       = S_OK;
  IAccessible* p_accobj = NULL;
  VARIANT      button;

  // Get toolbar accessibility object.
  IAccessible* p_toolbar = NULL;
  GetToolbarWnd(&p_toolbar);
  ASSERT_TRUE(NULL != p_toolbar);

  // Check button and it's Name, Role, State.
  hr = GetForwardButton(&p_accobj, &button);
  // It is not complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // Read it's properties.
  EXPECT_EQ(L"Forward", GetName(p_toolbar, button));
  EXPECT_EQ(L"drop down button", GetRole(p_toolbar, button));
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
  }
  CHK_RELEASE(p_accobj);

  CHK_RELEASE(p_toolbar);
}

// Check Back button and their Name, Role, State.
// This test is disabled. See bug 1119183.
TEST_F(AccessibilityTest, DISABLED_TestForwardBtnStatusOnNewTab) {
  HRESULT      hr       = S_OK;
  IAccessible* p_accobj = NULL;
  VARIANT      button;

  // Get toolbar accessibility object.
  IAccessible* p_toolbar = NULL;
  GetToolbarWnd(&p_toolbar);
  ASSERT_TRUE(NULL != p_toolbar);

  // Check button and it's Name, Role, State.
  hr = GetForwardButton(&p_accobj, &button);
  // It is not complete accessible object, as it is element.
  ASSERT_TRUE(S_FALSE == hr);
  ASSERT_TRUE(NULL == p_accobj);
  ASSERT_TRUE(VT_I4 == button.vt);
  // State "has popup" only supported in XP and higher.
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
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
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
  }
  // Go Back and check status.
  window->ApplyAccelerator(IDC_BACK);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable", GetState(p_toolbar, button));
  }
  // Go Forward and check status.
  window->ApplyAccelerator(IDC_FORWARD);
  Sleep(kWaitForActionMsec);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
  }

  // Add empty new tab and check status.
  ASSERT_TRUE(window->GetTabCount(&old_tab_count));
  ASSERT_TRUE(window->ApplyAccelerator(IDC_NEWTAB));
  ASSERT_TRUE(window->WaitForTabCountToChange(old_tab_count, &new_tab_count,
              5000));
  // Check tab count.
  ASSERT_GE(new_tab_count, old_tab_count);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
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
    EXPECT_EQ(L"has popup, focusable, unavailable",
              GetState(p_toolbar, button));
  } else {
    EXPECT_EQ(L"focusable, unavailable", GetState(p_toolbar, button));
  }

  CHK_RELEASE(p_toolbar);
}