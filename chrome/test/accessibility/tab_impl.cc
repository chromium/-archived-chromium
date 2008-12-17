// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/tab_impl.h"

#include <oleacc.h>

#include "chrome/test/accessibility/accessibility_util.h"
#include "chrome/test/accessibility/browser_impl.h"
#include "chrome/test/accessibility/keyboard_util.h"


TabImpl::~TabImpl() {
  if (tab_) {
    SysFreeString(tab_->title_);
    delete tab_;
  }
}

bool TabImpl::Close(void) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (!acc_obj || !hwnd)
    return false;

  // Activate main window and operate key Ctrl+F4.
  ActivateWnd(acc_obj, hwnd);
  ClickKey(hwnd, VK_CONTROL, VK_F4);
  CHK_RELEASE(acc_obj);

  // Update tab information in browser object.
  browser_->CloseTabFromCollection(tab_->index_);
  return true;
}

bool TabImpl::GetTitle(BSTR* title) {
  // Validation.
  if (!title)
    return false;

  // Read Tab name and store it in local member and return same value.
  BSTR tab_title = GetTabName(tab_->index_);
  *title = SysAllocString(tab_title);
  tab_->title_ = SysAllocString(tab_title);
  return true;
}

bool TabImpl::SetAddressBarText(const BSTR text) {
  IAccessible* acc_obj = NULL;
  HWND hwnd_addr_bar = GetAddressBarWnd(&acc_obj);
  if (!acc_obj || !hwnd_addr_bar)
    return false;

  // Activate address bar.
  ActivateWnd(acc_obj, hwnd_addr_bar);
  // Set text to address bar.
  SendMessage(hwnd_addr_bar, WM_SETTEXT, 0, LPARAM(text));
  CHK_RELEASE(acc_obj);
  return true;
}

bool TabImpl::NavigateToURL(const BSTR url) {
  IAccessible* acc_obj = NULL;
  HWND hwnd_addr_bar = GetAddressBarWnd(&acc_obj);

  if (!acc_obj || !hwnd_addr_bar)
    return false;

  // Activate address bar.
  ActivateWnd(acc_obj, hwnd_addr_bar);
  // Set text to address bar.
  SendMessage(hwnd_addr_bar, WM_SETTEXT, 0, LPARAM(url));
  // Click Enter. Window is activated above for this.
  ClickKey(hwnd_addr_bar, VK_RETURN);
  CHK_RELEASE(acc_obj);
  return true;
}

bool TabImpl::FindInPage(const BSTR find_text) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (!acc_obj || !hwnd)
    return false;

  // Activate main window and operate key 'F3' to invoke Find window.
  ActivateWnd(acc_obj, hwnd);
  ClickKey(hwnd, VK_F3);
  CHK_RELEASE(acc_obj);

  // If no text is to be searched, return.
  if (find_text != NULL) {
    // TODO(klink): Once FindWindow is exported through Accessibility.
    // Instead of sleep, check if FindWindows exists or not.
    Sleep(50);

    // Get Find window.
    acc_obj = NULL;
    hwnd = GetFindTextWnd(&acc_obj);
    if (hwnd) {
      HWND hwnd_find_edit = FindWindowEx(hwnd, 0, CHROME_VIEWS_TEXT_FIELD_EDIT,
                                         0);
      if (hwnd_find_edit) {
        ActivateWnd(acc_obj, hwnd);
        ActivateWnd(NULL, hwnd_find_edit);
        // Set text in Find window edit box.
        WCHAR* strTemp =
            reinterpret_cast<WCHAR*>(calloc(wcslen(find_text), sizeof(WCHAR)));
        wcscpy_s(strTemp, wcslen(find_text), find_text);
        for (size_t i = 0; i < wcslen(strTemp); i++) {
          SendMessage(hwnd_find_edit, WM_KEYDOWN, strTemp[i], 0);
          SendMessage(hwnd_find_edit, WM_CHAR, strTemp[i], 0);
          SendMessage(hwnd_find_edit, WM_KEYUP, strTemp[i], 0);
        }
      }
    }
  }
  CHK_RELEASE(acc_obj);

  return true;
}

bool TabImpl::Reload(void) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (!acc_obj || !hwnd)
    return false;

  // Operate key F5.
  ActivateWnd(acc_obj, hwnd);
  ClickKey(hwnd, VK_F5);
  CHK_RELEASE(acc_obj);
  return true;
}

bool TabImpl::Duplicate(TabImpl** tab) {
  return true;
}

bool TabImpl::IsAuthDialogVisible() {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (!acc_obj || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(acc_obj, hwnd);
  CHK_RELEASE(acc_obj);

  // Check for Authentication Window.
  acc_obj = NULL;
  hwnd = GetAuthWnd(&acc_obj);
  if (!hwnd || !acc_obj) {
      CHK_RELEASE(acc_obj);
      return false;
  }
  CHK_RELEASE(acc_obj);
  return true;
}

bool TabImpl::SetAuthDialog(const BSTR user_name, const BSTR password) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (!acc_obj || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(acc_obj, hwnd);
  CHK_RELEASE(acc_obj);

  // Get editbox for user name and password.
  acc_obj = NULL;
  hwnd = GetAuthWnd(&acc_obj);
  if (!hwnd) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Get handle to password edit box.
  HWND hwnd_auth_pwd = FindWindowEx(hwnd, 0, CHROME_VIEWS_TEXT_FIELD_EDIT, 0);
  if (!hwnd_auth_pwd) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Child after password edit box is edit box for name.
  HWND hwnd_auth_name = FindWindowEx(hwnd, hwnd_auth_pwd,
                                     CHROME_VIEWS_TEXT_FIELD_EDIT, 0);
  if (!hwnd_auth_name) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Activate Tab.
  SetActiveWindow(GetParent(hwnd));
  // Activate Authentication window.
  ActivateWnd(acc_obj, hwnd);

  // Activate edit box for name.
  ActivateWnd(NULL, hwnd_auth_name);

  // Set user name.
  if (user_name != NULL) {
    WCHAR* strTemp =
        reinterpret_cast<WCHAR*>(calloc(wcslen(user_name), sizeof(WCHAR)));
    wcscpy_s(strTemp, wcslen(user_name), user_name);
    for (size_t i = 0; i < wcslen(strTemp); i++) {
      SendMessage(hwnd_auth_name, WM_KEYDOWN, strTemp[i], 0);
      SendMessage(hwnd_auth_name, WM_CHAR, strTemp[i], 0);
      SendMessage(hwnd_auth_name, WM_KEYUP, strTemp[i], 0);
    }
  }

  // Activate edit box for password.
  ActivateWnd(NULL, hwnd_auth_pwd);

  // Set password.
  if (password != NULL) {
    // set text
    WCHAR* strTemp = reinterpret_cast<WCHAR*>(calloc(wcslen(password),
                                                     sizeof(WCHAR)));
    wcscpy_s(strTemp, wcslen(password), password);
    for (size_t i = 0; i < wcslen(strTemp); i++) {
      SendMessage(hwnd_auth_pwd, WM_KEYDOWN, strTemp[i], 0);
      SendMessage(hwnd_auth_pwd, WM_CHAR, strTemp[i], 0);
      SendMessage(hwnd_auth_pwd, WM_KEYUP, strTemp[i], 0);
    }
  }

  CHK_RELEASE(acc_obj);
  return true;
}

bool TabImpl::CancelAuthDialog(void) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (!acc_obj || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(acc_obj, hwnd);
  CHK_RELEASE(acc_obj);

  // Get editbox for user name which is after password.
  acc_obj = NULL;
  hwnd = GetAuthWnd(&acc_obj);
  if (!hwnd) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Get Cancel button.
  HWND cancel_button_container = FindWindowEx(hwnd, 0,
                                              CHROME_VIEWS_NATIVE_CTRL_CONTNR,
                                              0);
  if (!cancel_button_container) {
    CHK_RELEASE(acc_obj);
    return false;
  }
  HWND cancel_button = FindWindowEx(cancel_button_container, 0, STD_BUTTON, 0);
  if (!cancel_button) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Click Cancel button.
  SetActiveWindow(cancel_button_container);
  SetActiveWindow(cancel_button);
  SendMessage(cancel_button, BM_CLICK, 0, 0);

  CHK_RELEASE(acc_obj);
  return true;
}

bool TabImpl::UseAuthDialog(void) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (!acc_obj || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(acc_obj, hwnd);
  CHK_RELEASE(acc_obj);

  // Get editbox for user name which is after password.
  acc_obj = NULL;
  hwnd = GetAuthWnd(&acc_obj);
  if (!hwnd) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Get Ok button.
  HWND cancel_button_container = FindWindowEx(hwnd, 0,
                                              CHROME_VIEWS_NATIVE_CTRL_CONTNR,
                                              0);
  if (!cancel_button_container) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Ok button is located after cancel button in window hierarchy.
  HWND ok_button_container = FindWindowEx(hwnd, cancel_button_container,
                                          CHROME_VIEWS_NATIVE_CTRL_CONTNR, 0);
  if (!ok_button_container) {
    CHK_RELEASE(acc_obj);
    return false;
  }
  HWND ok_button = FindWindowEx(ok_button_container, 0, STD_BUTTON, 0);
  if (!ok_button) {
    CHK_RELEASE(acc_obj);
    return false;
  }

  // Click Ok.
  SetActiveWindow(ok_button_container);
  SetActiveWindow(ok_button);
  SendMessage(ok_button, BM_CLICK, 0, 0);

  CHK_RELEASE(acc_obj);
  return true;
}

void TabImpl::set_title(BSTR title) {
  if (!tab_)
    InitTabData();
  tab_->title_ = SysAllocString(title);
}

bool TabImpl::Activate(void) {
  return true;
}

bool TabImpl::WaitForTabToBecomeActive(const INT64 interval,
                                       const INT64 timeout) {
  return true;
}

bool TabImpl::WaitForTabToGetLoaded(const INT64 interval, const INT64 timeout) {
  return true;
}

bool TabImpl::IsSSLLockPresent(bool* present) {
  return true;
}

bool TabImpl::IsSSLSoftError(bool* soft_err) {
  return true;
}

bool TabImpl::OpenPageCertificateDialog(void) {
  return true;
}

bool TabImpl::ClosePageCertificateDialog(void) {
  return true;
}

bool TabImpl::GoBack(void) {
  return true;
}

bool TabImpl::GoForward(void) {
  return true;
}

