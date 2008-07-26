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

#include "chrome/test/accessibility/browser_impl.h"

#include <shellapi.h>

#include "chrome/test/accessibility/accessibility_util.h"
#include "chrome/test/accessibility/keyboard_util.h"
#include "chrome/test/accessibility/registry_util.h"

bool CBrowserImpl::Launch(void) {
  // TODO: Check if chrome already running.
  BSTR chrome_path  = SysAllocString(GetChromeExePath());
  BOOL  bool_return = FALSE;

  // Initialize and fill up structure.
  SHELLEXECUTEINFO shell_execute_info;
  memset(&shell_execute_info, 0, sizeof(SHELLEXECUTEINFO));
  shell_execute_info.cbSize = sizeof(SHELLEXECUTEINFO);
  // To get Process handle.
  shell_execute_info.fMask  = SEE_MASK_NOCLOSEPROCESS;
  shell_execute_info.nShow  = SW_SHOW;
  shell_execute_info.lpFile =
      reinterpret_cast<TCHAR*>(malloc(sizeof(TCHAR) *
                                      SysStringLen(chrome_path)));
  _tcscpy_s((TCHAR*)(shell_execute_info.lpFile), SysStringLen(chrome_path),
            chrome_path);

  // Execute.
  bool_return = ShellExecuteEx(&shell_execute_info);

  if (bool_return &&
      (INT64(shell_execute_info.hInstApp) > 32) ) {
    // TODO: Maintain instance and process handle.

    // Maintain active tab index.
    SetActiveTabIndex(1);

    // Create initial tab collection.
    UpdateTabCollection();

    // Chrome launched.
    return true;
  }

  return false;
}

bool CBrowserImpl::Quit(void) {
  // Cleanup.
  EraseTabCollection();

  // Send close message to browser window.
  HWND hwnd = GetChromeBrowserWnd(NULL);
  if (!hwnd)
    return false;
  SendMessage(hwnd, WM_CLOSE, 0, 0);
  return true;
}

bool CBrowserImpl::ActivateTab(const INT64 index) {
  // Validate index specified.
  if (index < 1) {
    return false;
  }

  // Goto next tab till focused at desired tab.
  // TODO: Change implementation when DoDefaultAction() for Tab is exported.
  while (active_tab_index_ != index) {
    GoToNextTab(NULL);
  }
  return true;
}

bool CBrowserImpl::GetActiveTabURL(BSTR* url) {
  // Validate input.
  if (!url)
    return false;

  // TODO: Implement.
  return true;
}

bool CBrowserImpl::GetActiveTabTitle(BSTR* title) {
  if (!title)
    return false;

  BSTR tab_title = SysAllocString(GetTabName(active_tab_index_));
  *title = SysAllocString(tab_title);
  return true;
}

bool CBrowserImpl::GetActiveTabIndex(INT64* index) {
  if (!index)
    return false;

  *index = active_tab_index_;
  return true;
}

void CBrowserImpl::SetActiveTabIndex(INT64 index) {
  if ((index >= MIN_TAB_INDEX_DIGIT) && (index <= GetTabCnt()))
    active_tab_index_ = index;
  return;
}

bool CBrowserImpl::GetActiveTab(CTabImpl** tab) {
  return GetTab(active_tab_index_, tab);
}

bool CBrowserImpl::GetTabCount(INT64* count) {
  if (!count)
    return false;

  *count = GetTabCnt();
  return true;
}

bool CBrowserImpl::GetBrowserProcessCount(INT64* count) {
  if (!count)
    return false;

  // TODO: Add your implementation code here

  return true;
}

bool CBrowserImpl::GetBrowserTitle(BSTR* title) {
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

bool CBrowserImpl::AddTab(CTabImpl** tab) {
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
  CTabImpl *new_tab = new CTabImpl();
  if (!new_tab)
    return false;
  ChromeTab* tab_data = new_tab->InitTabData();
  new_tab->PutIndex(new_tab_index);
  new_tab->PutTitle(GetTabName(new_tab_index));
  new_tab->SetBrowser(this);

  // Update tab collection.
  tab_collection_.push_back(tab_data);

  // Create tab object, if requested.
  if (tab)
    *tab = new_tab;

  return true;
}

bool CBrowserImpl::GetTab(const INT64 index, CTabImpl** tab) {
  // Create tab object, if requested.
  if (!tab)
    return false;

  if (index > GetTabCnt())
    return false;

  *tab = new CTabImpl();
  if (!*tab)
    return false;

  // Fill object.
  ChromeTab* tab_data = (*tab)->InitTabData();
  (*tab)->PutIndex(index);
  (*tab)->PutTitle(GetTabName(index));
  (*tab)->SetBrowser(this);

  return true;
}

bool CBrowserImpl::GoToTab(const INT64 index, CTabImpl** tab) {
  // Validate input.
  if (index > MAX_TAB_INDEX_DIGIT)
    return false;

  // Stay on current tab, if index doesnot exist.
  if ((0 == index) || (GetTabCnt() < index))
    return true;

  // Move to a tab (indexed 1 to 9).
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (pi_access && hwnd) {
    // Activate main window and operate key Ctrl+digit.
    ActivateWnd(pi_access, hwnd);
    ClickKey(hwnd, VK_CONTROL, WORD('0'+index));
    CHK_RELEASE(pi_access);

    // Set focused tab index.
    active_tab_index_ = index;
    // Return tab object.
    if (tab) {
      return GetTab(active_tab_index_, tab);
    }
  }

  return false;
}

bool CBrowserImpl::GoToNextTab(CTabImpl** tab) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (pi_access && hwnd) {
    // Activate main window and operate key Ctrl+Tab.
    ActivateWnd(pi_access, hwnd);
    ClickKey(hwnd, VK_CONTROL, VK_TAB);
    CHK_RELEASE(pi_access);

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

bool CBrowserImpl::GoToPrevTab(CTabImpl** tab) {
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (pi_access && hwnd) {
    // Activate main window and operate key Ctrl+Tab.
    ActivateWnd(pi_access, hwnd);
    ClickKey(hwnd, VK_SHIFT, VK_CONTROL, VK_TAB);
    CHK_RELEASE(pi_access);

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

bool CBrowserImpl::WaitForChromeToBeVisible(const INT64 interval,
                                            const INT64 timeout,
                                            bool* visible) {
  IAccessible *pi_access = NULL;
  INT64 time_elapsed = 0;
  *visible = false;

  // Check and wait.
  while (timeout >= time_elapsed) {
    GetTabStripWnd(&pi_access);
    if (pi_access) {
      *visible = true;
      CHK_RELEASE(pi_access);
      return true;
    }
    Sleep(DWORD(interval));
    time_elapsed = time_elapsed + interval;
  }

  return false;
}

bool CBrowserImpl::WaitForTabCountToChange(const INT64 interval,
                                           const INT64 timeout,
                                           bool* changed) {
  // TODO: Add your implementation code here

  return true;
}

bool CBrowserImpl::WaitForTabToBecomeActive(const INT64 index,
                                            const INT64 interval,
                                            const INT64 timeout,
                                            bool* activated) {
  // TODO: Add your implementation code here

  return true;
}

bool CBrowserImpl::ApplyAccelerator(VARIANT keys) {
  // Input should be -array of enum or strings
  // or -IDispatch (jscript array object).
  if ((keys.vt != (VT_ARRAY|VT_BSTR))  &&   // Array of string values.
      (keys.vt != (VT_ARRAY|VT_I4))  &&     // Array of enum values.
      (!(keys.vt & VT_DISPATCH)) ) {        // Object.
    return false;
  }

  // Array to store keys in a single combination. Currently, valid keyboard
  // -input combination can constitute of at the most 3 keys.
  KEYBD_KEYS  key_value[3];
  // Initialize key count.
  int key_cnt = 0;
  // Get variant array from object.
  IDispatch *disp = NULL;

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
    SAFEARRAY  *key_safe = NULL;
    key_safe = V_ARRAY(&keys);

    // Operate on Variant Array.
    HRESULT hr = S_OK;
    LONG    cElements, lLBound, lUBound;

    // Array is not 1-dimentional.
    if (SafeArrayGetDim(key_safe) != 1)
      return false;

    // Get array bounds.
    hr = SafeArrayGetLBound(key_safe, 1, &lLBound);
    if (S_OK !=hr)
      return false;
    hr = SafeArrayGetUBound(key_safe, 1, &lUBound);
    if (S_OK !=hr)
      return false;

    // Key combination can be of maximum 3 keys.
    cElements = lUBound-lLBound+1;
    if (cElements > 3)
      return false;
    key_cnt = cElements;

    // Read the data in array.
    if (keys.vt == (VT_ARRAY|VT_I4)) {
      KEYBD_KEYS *read_keys;
      hr = SafeArrayAccessData(key_safe,
                               reinterpret_cast<void **>(&read_keys));
      if (S_OK !=hr)
        return false;
      for (int i = 0; i < cElements; i++) {
        key_value[i] = read_keys[i];
      }
    } else if (keys.vt == (VT_ARRAY|VT_BSTR)) {
      BSTR *key_str_value;
      hr = SafeArrayAccessData(key_safe,
                               reinterpret_cast<void **>(&key_str_value));
      if (S_OK !=hr)
        return false;

      // Translate and add key to array.
      for (int i = 0; i < cElements; i++) {
        key_value[i] = GetKeybdKeysVal(key_str_value[i]);
      }
    }
  }

  // Focus on main window and operate keys.
  IAccessible *pi_access = NULL;
  HWND hwnd = GetChromeBrowserWnd(&pi_access);
  if (pi_access || hwnd)
    ActivateWnd(pi_access, hwnd);

  if (1 == key_cnt)
    ClickKey(hwnd, key_value[0]);
  else if (2 == key_cnt)
    ClickKey(hwnd, key_value[0], key_value[1]);
  else if (3 == key_cnt)
    ClickKey(hwnd, key_value[0], key_value[1], key_value[2]);

  CHK_RELEASE(pi_access);

  return true;
}

void CBrowserImpl::UpdateTabCollection(void) {
  // Get tab count and browser title.
  INT64 tab_cnt = GetTabCnt();
  BSTR browser_title;
  GetBrowserTitle(&browser_title);

  // Check tab-collection size and no. of existing tabs,
  // work accordingly.

  // First time creation
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

  // TODO: If tabs are swapped.
  // Add implementation here.
}

void CBrowserImpl::EraseTabCollection(void) {
  std::vector<ChromeTab*>::iterator tab_iterator;
  for (tab_iterator = tab_collection_.begin();
       tab_iterator != tab_collection_.end();
       tab_iterator++) {
    // Relese memory used for data.
    CHK_DELETE(*tab_iterator);
  }
  tab_collection_.clear();
}

void CBrowserImpl::CloseTabFromCollection(INT64 index) {
  std::vector <ChromeTab*>::size_type collection_size = tab_collection_.size();
  // Validate tab index.
  if ((index < MIN_TAB_INDEX_DIGIT) ||
      (static_cast<unsigned int>(index) > collection_size) )
    return;

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
