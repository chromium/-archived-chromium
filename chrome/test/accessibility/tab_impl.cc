// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_impl.h"
#include "tab_impl.h"
#include "accessibility_util.h"
#include "keyboard_util.h"
#include "constants.h"

bool CTabImpl::Close(void) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (!pi_access || !hwnd)
    return false;

  // Activate main window and operate key Ctrl+F4.
  ActivateWnd(pi_access, hwnd);
  ClickKey(hwnd, VK_CONTROL, VK_F4);
  CHK_RELEASE(pi_access);

  // Update tab information in browser object.
  my_browser_->CloseTabFromCollection(tab_->index_);
  return true;
}

bool CTabImpl::GetTitle(BSTR* title) {
  // Validation.
  if (!title)
    return false;

  // Read Tab name and store it in local member and return same value.
  BSTR tab_title = GetTabName(tab_->index_);
  *title = SysAllocString(tab_title);
  tab_->title_ = SysAllocString(tab_title);
  return true;
}

bool CTabImpl::SetAddressBarText(const BSTR text) {
  IAccessible *pi_access = NULL;
  HWND hwnd_addr_bar = GetAddressBarWnd(&pi_access);
  if (!pi_access || !hwnd_addr_bar)
    return false;

  // Activate address bar.
  ActivateWnd(pi_access, hwnd_addr_bar);
  // Set text to address bar.
  SendMessage(hwnd_addr_bar, WM_SETTEXT, 0, LPARAM(text));
  CHK_RELEASE(pi_access);
  return true;
}

bool CTabImpl::NavigateToURL(const BSTR url) {
  IAccessible *pi_access = NULL;
  HWND hwnd_addr_bar = GetAddressBarWnd(&pi_access);

  if (!pi_access || !hwnd_addr_bar)
    return false;

  // Activate address bar.
  ActivateWnd(pi_access, hwnd_addr_bar);
  // Set text to address bar.
  SendMessage(hwnd_addr_bar, WM_SETTEXT, 0, LPARAM(url));
  // Click Enter. Window is activated above for this.
  ClickKey(hwnd_addr_bar, VK_RETURN);
  CHK_RELEASE(pi_access);
  return true;
}

bool CTabImpl::FindInPage(const BSTR find_text) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (!pi_access || !hwnd)
    return false;

  // Activate main window and operate key 'F3' to invoke Find window.
  ActivateWnd(pi_access, hwnd);
  ClickKey(hwnd, VK_F3);
  CHK_RELEASE(pi_access);

  // If no text is to be searched, return.
  if (find_text != NULL) {
    // TODO: Once FindWindow is exported through Accessibility.
    // Instead of sleep, check if FindWindows exists or not.
    Sleep(50);

    // Get Find window.
    pi_access = NULL;
    hwnd = GetFindTextWnd(&pi_access);
    if (hwnd) {
      HWND hwnd_find_edit = FindWindowEx(hwnd, 0,
                                         CHROME_VIEWS_TEXT_FIELD_EDIT, 0);
      if (hwnd_find_edit) {
        ActivateWnd(pi_access, hwnd);
        ActivateWnd(NULL, hwnd_find_edit);
        // Set text in Find window edit box.
        WCHAR* strTemp =
            reinterpret_cast<WCHAR*>(calloc(wcslen(find_text), sizeof(WCHAR)));
        wcscpy_s(strTemp, wcslen(find_text), find_text);
        for (size_t i = 0; i < wcslen(strTemp); i++) {
          SendMessage(hwnd_find_edit, WM_KEYDOWN, strTemp[i], 0);
          SendMessage(hwnd_find_edit, WM_CHAR,  strTemp[i], 0);
          SendMessage(hwnd_find_edit, WM_KEYUP,  strTemp[i], 0);
        }
      }
    }
  }
  CHK_RELEASE(pi_access);

  return true;
}

bool CTabImpl::Reload(void) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (!pi_access || !hwnd)
    return false;

  // Operate key F5.
  ActivateWnd(pi_access, hwnd);
  ClickKey(hwnd, VK_F5);
  CHK_RELEASE(pi_access);
  return true;
}

bool CTabImpl::Duplicate(CTabImpl** tab) {
  // TODO: Add your implementation code here
  return true;
}

bool CTabImpl::IsAuthDialogVisible() {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (!pi_access || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(pi_access, hwnd);
  CHK_RELEASE(pi_access);

  // Check for Authentication Window.
  pi_access = NULL;
  hwnd = GetAuthWnd(&pi_access);
  if (!hwnd || !pi_access) {
      CHK_RELEASE(pi_access);
      return false;
    }
  return true;
}

bool CTabImpl::SetAuthDialog(const BSTR user_name, const BSTR password) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (!pi_access || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(pi_access, hwnd);
  CHK_RELEASE(pi_access);

  // Get editbox for user name and password.
  pi_access = NULL;
  hwnd = GetAuthWnd(&pi_access);
  if (!hwnd) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Get handle to password edit box.
  HWND hwnd_auth_pwd = FindWindowEx(hwnd, 0, CHROME_VIEWS_TEXT_FIELD_EDIT, 0);
  if (!hwnd_auth_pwd) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Child after password edit box is edit box for name.
  HWND hwnd_auth_name = FindWindowEx(hwnd, hwnd_auth_pwd,
                                     CHROME_VIEWS_TEXT_FIELD_EDIT, 0);
  if (!hwnd_auth_name) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Activate Tab.
  SetActiveWindow(GetParent(hwnd));
  // Activate Authentication window.
  ActivateWnd(pi_access, hwnd);

  // Activate edit box for name.
  ActivateWnd(NULL, hwnd_auth_name);

  // Set user name.
  if (user_name != NULL) {
    WCHAR* strTemp =
        reinterpret_cast<WCHAR*>(calloc(wcslen(user_name), sizeof(WCHAR)));
    wcscpy_s(strTemp, wcslen(user_name), user_name);
    for (size_t i = 0; i < wcslen(strTemp); i++) {
      SendMessage(hwnd_auth_name, WM_KEYDOWN, strTemp[i], 0);
      SendMessage(hwnd_auth_name, WM_CHAR,  strTemp[i], 0);
      SendMessage(hwnd_auth_name, WM_KEYUP,  strTemp[i], 0);
    }
  }

  // Activate edit box for password.
  ActivateWnd(NULL, hwnd_auth_pwd);

  // Set password.
  if (password != NULL) {
    // set text
    WCHAR* strTemp =
        reinterpret_cast<WCHAR*>(calloc(wcslen(password), sizeof(WCHAR)));
    wcscpy_s(strTemp, wcslen(password), password);
    for (size_t i = 0; i < wcslen(strTemp); i++) {
      SendMessage(hwnd_auth_pwd, WM_KEYDOWN, strTemp[i], 0);
      SendMessage(hwnd_auth_pwd, WM_CHAR, strTemp[i], 0);
      SendMessage(hwnd_auth_pwd, WM_KEYUP, strTemp[i], 0);
    }
  }

  CHK_RELEASE(pi_access);
  return true;
}

bool CTabImpl::CancelAuthDialog(void) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (!pi_access || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(pi_access, hwnd);
  CHK_RELEASE(pi_access);

  // Get editbox for user name which is after password.
  pi_access = NULL;
  hwnd = GetAuthWnd(&pi_access);
  if (!hwnd) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Get Cancel button.
  HWND cancel_button_container =
      FindWindowEx(hwnd, 0, CHROME_VIEWS_NATIVE_CTRL_CONTNR, 0);
  if (!cancel_button_container) {
    CHK_RELEASE(pi_access);
    return false;
  }
  HWND cancel_button = FindWindowEx(cancel_button_container, 0, STD_BUTTON, 0);
  if (!cancel_button) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Click Cancel button.
  SetActiveWindow(cancel_button_container);
  SetActiveWindow(cancel_button);
  SendMessage(cancel_button, BM_CLICK, 0, 0);

  return true;
}

bool CTabImpl::UseAuthDialog(void) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (!pi_access || !hwnd)
    return false;

  // Activate main window.
  ActivateWnd(pi_access, hwnd);
  CHK_RELEASE(pi_access);

  // Get editbox for user name which is after password.
  pi_access = NULL;
  hwnd = GetAuthWnd(&pi_access);
  if (!hwnd) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Get Ok button.
  HWND cancel_button_container =
      FindWindowEx(hwnd, 0, CHROME_VIEWS_NATIVE_CTRL_CONTNR, 0);
  if (!cancel_button_container) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Ok button is located after cancel button in window hierarchy.
  HWND ok_button_container = FindWindowEx(hwnd, cancel_button_container,
                                          CHROME_VIEWS_NATIVE_CTRL_CONTNR, 0);
  if (!ok_button_container) {
    CHK_RELEASE(pi_access);
    return false;
  }
  HWND ok_button = FindWindowEx(ok_button_container, 0, STD_BUTTON, 0);
  if (!ok_button) {
    CHK_RELEASE(pi_access);
    return false;
  }

  // Click Ok.
  SetActiveWindow(ok_button_container);
  SetActiveWindow(ok_button);
  SendMessage(ok_button, BM_CLICK, 0, 0);

  return true;
}

bool CTabImpl::Activate(void) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::WaitForTabToBecomeActive(const INT64 interval,
                                        const INT64 timeout) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::WaitForTabToGetLoaded(const INT64 interval,
                                     const INT64 timeout) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::IsSSLLockPresent(bool* present) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::IsSSLSoftError(bool* soft_err) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::OpenPageCertificateDialog(void) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::ClosePageCertificateDialog(void) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::GoBack(void) {
  // TODO: Add your implementation code here

  return true;
}

bool CTabImpl::GoForward(void) {
  // TODO: Add your implementation code here

  return true;
}

