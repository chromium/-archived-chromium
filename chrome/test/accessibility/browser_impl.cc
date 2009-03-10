// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/browser_impl.h"

#include <shellapi.h>

#include "chrome/test/accessibility/accessibility_util.h"
#include "chrome/test/accessibility/keyboard_util.h"
#include "chrome/test/accessibility/registry_util.h"

bool BrowserImpl::Launch(void) {
  // TODO(klink): Check if chrome already running.
  BSTR chrome_path = SysAllocString(GetChromeExePath());
  BOOL success = FALSE;

  // Initialize and fill up structure.
  SHELLEXECUTEINFO shell_execute_info;
  memset(&shell_execute_info, 0, sizeof(SHELLEXECUTEINFO));
  shell_execute_info.cbSize = sizeof(SHELLEXECUTEINFO);
  // To get Process handle.
  shell_execute_info.fMask = SEE_MASK_NOCLOSEPROCESS;
  shell_execute_info.nShow = SW_SHOW;
  shell_execute_info.lpFile =
      reinterpret_cast<TCHAR*>(malloc(sizeof(TCHAR) *
                                      SysStringLen(chrome_path)));
  _tcscpy_s((TCHAR*)(shell_execute_info.lpFile), SysStringLen(chrome_path),
            chrome_path);

  // Execute.
  success = ShellExecuteEx(&shell_execute_info);

  if (success && (INT64(shell_execute_info.hInstApp) > 32)) {
    // TODO(klink): Maintain instance and process handle.

    // Maintain active tab index.
    SetActiveTabIndex(1);

    // Create initial tab collection.
    UpdateTabCollection();

    // Chrome launched.
    SysFreeString(chrome_path);
    return true;
  }

  SysFreeString(chrome_path);
  return false;
}

bool BrowserImpl::Quit(void) {
  // Cleanup.
  EraseTabCollection();

  // Send close message to browser window.
  HWND hwnd = GetChromeBrowserWnd(NULL);
  if (!hwnd)
    return false;
  SendMessage(hwnd, WM_CLOSE, 0, 0);
  return true;
}

bool BrowserImpl::ActivateTab(const INT64 index) {
  // Validate index specified.
  if (index < 1)
    return false;

  // Goto next tab till focused at desired tab.
  while (active_tab_index_ != index) {
    GoToNextTab(NULL);
  }
  return true;
}

bool BrowserImpl::GetActiveTabURL(BSTR* url) {
  // Validate input.
  if (!url)
    return false;

  return true;
}

bool BrowserImpl::GetActiveTabTitle(BSTR* title) {
  if (!title)
    return false;

  *title = SysAllocString(GetTabName(active_tab_index_));
  return true;
}

bool BrowserImpl::GetActiveTabIndex(INT64* index) {
  if (!index)
    return false;

  *index = active_tab_index_;
  return true;
}

void BrowserImpl::SetActiveTabIndex(INT64 index) {
  if ((index >= MIN_TAB_INDEX_DIGIT) && (index <= GetTabCnt()))
    active_tab_index_ = index;
  return;
}

bool BrowserImpl::GetActiveTab(TabImpl** tab) {
  return GetTab(active_tab_index_, tab);
}

bool BrowserImpl::GetTabCount(INT64* count) {
  if (!count)
    return false;

  *count = GetTabCnt();
  return true;
}

bool BrowserImpl::GetBrowserProcessCount(INT64* count) {
  if (!count)
    return false;

  return true;
}

bool BrowserImpl::GetBrowserTitle(BSTR* title) {
  if (!title)
    return false;

  HWND hwnd = GetChromeBrowserWnd(NULL);
  if (!hwnd)
    return false;

  int text_length = GetWindowTextLength(hwnd);
  *title = SysAllocStringLen(NULL, text_length);
  GetWindowText(hwnd, *title, text_length);
  return true;
}

bool BrowserImpl::AddTab(TabImpl** tab) {
  // Add new tab.
  HWND hwnd = GetChromeBrowserWnd(NULL);
  if (!hwnd)
    return false;
  ClickKey(hwnd, VK_CONTROL, 'T');

  // Update active tab index.
  INT64 new_tab_index = GetTabCnt();
  if (-1 == new_tab_index)
    return false;
  SetActiveTabIndex(new_tab_index);

  // Fill object.
  TabImpl* new_tab = new TabImpl();
  if (!new_tab)
    return false;
  ChromeTab* tab_data = new_tab->InitTabData();
  new_tab->set_index(new_tab_index);
  new_tab->set_title(GetTabName(new_tab_index));
  new_tab->set_browser(this);

  // Create a copy for storage, in case the caller deletes this newly created
  // TabImpl before [tab_collection_] is done using [tab_data].
  ChromeTab* tab_data_copy = tab_data;

  // Update tab collection.
  tab_collection_.push_back(linked_ptr<ChromeTab>(tab_data_copy));

  // Create tab object, if requested.
  if (tab)
    *tab = new_tab;

  return true;
}

bool BrowserImpl::GetTab(const INT64 index, TabImpl** tab) {
  // Create tab object, if requested.
  if (!tab)
    return false;

  if (index > GetTabCnt())
    return false;

  *tab = new TabImpl();
  if (!*tab)
    return false;

  // Fill object.
  ChromeTab* tab_data = (*tab)->InitTabData();
  (*tab)->set_index(index);
  (*tab)->set_title(GetTabName(index));
  (*tab)->set_browser(this);

  return true;
}

bool BrowserImpl::GoToTab(const INT64 index, TabImpl** tab) {
  // Validate input.
  if (index > MAX_TAB_INDEX_DIGIT)
    return false;

  // Stay on current tab, if index doesnot exist.
  if ((0 == index) || (GetTabCnt() < index))
    return true;

  // Move to a tab (indexed 1 to 9).
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (acc_obj && hwnd) {
    // Activate main window and operate key Ctrl+digit.
    ActivateWnd(acc_obj, hwnd);
    ClickKey(hwnd, VK_CONTROL, WORD('0'+index));
    CHK_RELEASE(acc_obj);

    // Set focused tab index.
    active_tab_index_ = index;
    // Return tab object.
    if (tab) {
      return GetTab(active_tab_index_, tab);
    }
  }

  return false;
}

bool BrowserImpl::GoToNextTab(TabImpl** tab) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (acc_obj && hwnd) {
    // Activate main window and operate key Ctrl+Tab.
    ActivateWnd(acc_obj, hwnd);
    ClickKey(hwnd, VK_CONTROL, VK_TAB);
    CHK_RELEASE(acc_obj);

    // Set focused tab index.
    if (active_tab_index_ == GetTabCnt()) {
      active_tab_index_ = 1;
    } else {
      active_tab_index_ = active_tab_index_ + 1;
    }

    // Return tab object.
    if (tab) {
      return GetTab(active_tab_index_, tab);
    }
  }

  return false;
}

bool BrowserImpl::GoToPrevTab(TabImpl** tab) {
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (acc_obj && hwnd) {
    // Activate main window and operate key Ctrl+Tab.
    ActivateWnd(acc_obj, hwnd);
    ClickKey(hwnd, VK_SHIFT, VK_CONTROL, VK_TAB);
    CHK_RELEASE(acc_obj);

    // Set focused tab index.
    if (active_tab_index_ == 1) {
      active_tab_index_ = GetTabCnt();
    } else {
      active_tab_index_ = active_tab_index_ - 1;
    }

    // Return tab object.
    if (tab) {
      return GetTab(active_tab_index_, tab);
    }
  }

  return false;
}

bool BrowserImpl::WaitForChromeToBeVisible(const INT64 interval,
                                           const INT64 timeout, bool* visible) {
  IAccessible* acc_obj = NULL;
  INT64 time_elapsed = 0;
  *visible = false;

  // Check and wait.
  while (timeout >= time_elapsed) {
    GetTabStripAccessible(&acc_obj);
    if (acc_obj) {
      *visible = true;
      CHK_RELEASE(acc_obj);
      return true;
    }
    Sleep(DWORD(interval));
    time_elapsed = time_elapsed + interval;
  }

  return false;
}

bool BrowserImpl::WaitForTabCountToChange(const INT64 interval,
                                          const INT64 timeout, bool* changed) {
  return true;
}

bool BrowserImpl::WaitForTabToBecomeActive(const INT64 index,
                                           const INT64 interval,
                                           const INT64 timeout,
                                           bool* activated) {
  return true;
}

bool BrowserImpl::ApplyAccelerator(VARIANT keys) {
  // Input should be -array of enum or strings or -IDispatch (jscript array
  // object).
  if ((keys.vt != (VT_ARRAY|VT_BSTR))  &&   // Array of string values.
      (keys.vt != (VT_ARRAY|VT_I4))  &&     // Array of enum values.
      (!(keys.vt & VT_DISPATCH)) ) {        // Object.
    return false;
  }

  // Array to store keys in a single combination. Currently, valid keyboard
  // -input combination can constitute of at the most 3 keys.
  KEYBD_KEYS key_value[3];
  // Initialize key count.
  int key_cnt = 0;
  // Get variant array from object.
  IDispatch* disp = NULL;

  // Not array of string values or integers.
  if ((keys.vt != (VT_ARRAY|VT_BSTR)) &&
      (keys.vt != (VT_ARRAY|VT_I4))  ) {
    // Retrive IDispatch.
    if (keys.vt & VT_BYREF)
      disp = *(keys.ppdispVal);
    else
      disp = keys.pdispVal;

    // Get array length.
    DISPPARAMS params;
    FillMemory(&params, sizeof(DISPPARAMS), 0);
    VARIANT res;
    DISPID id;
    LPOLESTR ln = L"length";
    if (S_OK != disp->GetIDsOfNames(IID_NULL, &ln, 1, LOCALE_USER_DEFAULT,
                                    &id)) {
      return false;
    }

    if (S_OK != disp->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT,
                             DISPATCH_PROPERTYGET, &params, &res, NULL,
                             NULL)) {
      return false;
    }

    VARIANT len;
    VariantInit(&len);
    VariantChangeType(&len, &res, 0, VT_I4);
    if (len.lVal > 3)
      return false;
    key_cnt = len.lVal;

    // Add elements to safe array.
    for (int i = 0; i < len.lVal; i++) {
      // Fetch element.
      wchar_t wstr[5];
      memset(wstr, 0, 5*sizeof(wchar_t));
      wsprintf(wstr, L"%d", i);
      LPOLESTR olestr = wstr;

      if (S_OK != disp->GetIDsOfNames(IID_NULL, &olestr, 1,
                                      LOCALE_USER_DEFAULT, &id)) {
        return false;
      }

      if (S_OK != disp->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_PROPERTYGET, &params, &res, NULL,
                               NULL)) {
        return false;
      }

      VARIANT value;
      VariantInit(&value);
      VariantChangeType(&value, &res, 0, VT_BSTR);

      // Translate and add key to array.
      key_value[i] = GetKeybdKeysVal(value.bstrVal);

      VariantClear(&value);
    }

    VariantClear(&len);
  } else {
    // Directly fetch array.
    SAFEARRAY* key_safe = NULL;
    key_safe = V_ARRAY(&keys);

    // Operate on Variant Array.
    HRESULT hr = S_OK;
    LONG num_elements, lower_bound, upper_bound;

    // Array is not 1-dimentional.
    if (SafeArrayGetDim(key_safe) != 1)
      return false;

    // Get array bounds.
    hr = SafeArrayGetLBound(key_safe, 1, &lower_bound);
    if (S_OK !=hr)
      return false;
    hr = SafeArrayGetUBound(key_safe, 1, &upper_bound);
    if (S_OK !=hr)
      return false;

    // Key combination can be of maximum 3 keys.
    num_elements = upper_bound - lower_bound + 1;
    if (num_elements > 3)
      return false;
    key_cnt = num_elements;

    // Read the data in array.
    if (keys.vt == (VT_ARRAY|VT_I4)) {
      KEYBD_KEYS* read_keys;
      hr = SafeArrayAccessData(key_safe, reinterpret_cast<void **>(&read_keys));
      if (S_OK != hr)
        return false;
      for (int i = 0; i < num_elements; i++) {
        key_value[i] = read_keys[i];
      }
    } else if (keys.vt == (VT_ARRAY|VT_BSTR)) {
      BSTR* key_str_value;
      hr = SafeArrayAccessData(key_safe,
                               reinterpret_cast<void **>(&key_str_value));
      if (S_OK != hr)
        return false;

      // Translate and add key to array.
      for (int i = 0; i < num_elements; i++) {
        key_value[i] = GetKeybdKeysVal(key_str_value[i]);
      }
    }
  }

  // Focus on main window and operate keys.
  IAccessible* acc_obj = NULL;
  HWND hwnd = GetChromeBrowserWnd(&acc_obj);
  if (acc_obj || hwnd)
    ActivateWnd(acc_obj, hwnd);

  if (1 == key_cnt)
    ClickKey(hwnd, key_value[0]);
  else if (2 == key_cnt)
    ClickKey(hwnd, key_value[0], key_value[1]);
  else if (3 == key_cnt)
    ClickKey(hwnd, key_value[0], key_value[1], key_value[2]);

  CHK_RELEASE(acc_obj);

  return true;
}

void BrowserImpl::UpdateTabCollection(void) {
  // Get tab count and browser title.
  INT64 tab_cnt = GetTabCnt();
  BSTR browser_title;
  GetBrowserTitle(&browser_title);

  // Check tab-collection size and number of existing tabs, work accordingly.

  // First time creation.
  if (0 == tab_collection_.size()) {
    EraseTabCollection();
    for (int i = 0; i < tab_cnt; i++) {
      tab_collection_[i]->index_ = i + 1;
      tab_collection_[i]->title_ =
          SysAllocString(GetTabName(tab_collection_[i]->index_));
      if (browser_title == tab_collection_[i]->title_) {
        active_tab_index_ = tab_collection_[i]->index_;
      }
    }
  }
  SysFreeString(browser_title);

  // TODO(klink): Add implementation here to handle if tabs are reordered,
  // rather than created.
}

void BrowserImpl::EraseTabCollection(void) {
  tab_collection_.clear();
}

void BrowserImpl::CloseTabFromCollection(INT64 index) {
  std::vector <ChromeTab*>::size_type collection_size = tab_collection_.size();
  // Validate tab index.
  if ((index < MIN_TAB_INDEX_DIGIT) ||
      (static_cast<unsigned int>(index) > collection_size)) {
    return;
  }

  // Index starts from 1.
  tab_collection_.erase(tab_collection_.begin() + static_cast<int>(index) - 1);

  // Now update tab collection data.
  collection_size = tab_collection_.size();

  // Check if tab deleted is last tab.
  if (index-1 == collection_size) {
    // Change active tab index, only if tab deleted is last tab.
    active_tab_index_ = index - 1;
  } else {
    for (std::vector <ChromeTab*>::size_type i =
         static_cast<unsigned int>(index) - 1;
         i < collection_size;
         i++) {
      tab_collection_[i]->index_--;
    }
  }
}
