// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/accessibility_util.h"

#include "base/win_util.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/l10n_util.h"
#include "chrome/test/accessibility/constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

VARIANT id_self = {VT_I4, CHILDID_SELF};

// TODO(beng): clean this up
static const wchar_t* kBrowserWindowKey = L"__BROWSER_WINDOW__";

static BOOL CALLBACK WindowEnumProc(HWND hwnd, LPARAM data) {
  std::wstring class_name = win_util::GetClassName(hwnd);
  if (class_name == CHROME_HWND_VIEW_CONTAINER) {
    HANDLE window_interface = GetProp(hwnd, kBrowserWindowKey);
    if (window_interface) {
      HWND* out = reinterpret_cast<HWND*>(data);
      *out = hwnd;
      return FALSE;
    }
  }
  return TRUE;
}

HWND GetChromeBrowserWnd(IAccessible** acc_obj) {
  HWND hwnd = NULL;

  EnumWindows(WindowEnumProc, reinterpret_cast<LPARAM>(&hwnd));

  if (!hwnd) {
    CHK_RELEASE(*acc_obj);
    return NULL;
  }

  // Get accessibility object for Chrome, only if requested (not NULL).
  if (!acc_obj)
    return hwnd;

  *acc_obj = NULL;

  // Get accessibility object for Chrome Main Window. If failed to get it,
  // return only window handle.
  IAccessible* root_acc_obj = NULL;
  HRESULT hr = S_OK;
  hr = AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                  reinterpret_cast<void**>(&root_acc_obj));
  if ((S_OK != hr) || !root_acc_obj)
    return hwnd;

  const std::wstring product_name = l10n_util::GetString(IDS_PRODUCT_NAME);
  BSTR name;

  // Confirm if it is Chrome Main Window using its accessibility object's
  // Name and Role property. If it's not the desired object, return only
  // window handle.
  hr = root_acc_obj->get_accName(id_self, &name);
  if ((S_OK != hr) || (!name) ||
      (0 != _wcsicmp(name, product_name.c_str())) ) {
    CHK_RELEASE(root_acc_obj);
    return hwnd;
  }
  if (ROLE_SYSTEM_WINDOW != GetRole(root_acc_obj)) {
    CHK_RELEASE(root_acc_obj);
    return hwnd;
  }

  // Get accessibility child objects for Chrome Main Window. If failed, return
  // only window handle.
  INT64 child_cnt = GetChildCount(root_acc_obj);
  VARIANT* children = reinterpret_cast<VARIANT*>(calloc(size_t(child_cnt),
                                                        sizeof(VARIANT)));

  if (!children) {
    CHK_RELEASE(root_acc_obj);
    return hwnd;
  }

  hr = GetChildrenArray(root_acc_obj, children);
  if (S_OK != hr) {
    CHK_RELEASE(root_acc_obj);
    free(children);
    return hwnd;
  }

  // Fetch desired child (Chrome App Window) of Chrome Main Window.
  IAccessible* app_acc_obj = NULL;
  GetChildAccObject(root_acc_obj, children[CHROME_APP_ACC_INDEX], &app_acc_obj);
  if (!app_acc_obj) {
    CHK_RELEASE(app_acc_obj);
    free(children);
    return hwnd;
  }

  // Confirm if it is Chrome App Window by using it's accessibility object's
  // Name and Role property. If it's not the desired object, return only
  // window handle.
  hr = app_acc_obj->get_accName(id_self, &name);
  if ((S_OK != hr) || (!name) ||
      (0 != _wcsicmp(name, product_name.c_str())) ) {
    CHK_RELEASE(app_acc_obj);
    CHK_RELEASE(root_acc_obj);
    free(children);
    return hwnd;
  }
  if (ROLE_SYSTEM_APPLICATION != GetRole(app_acc_obj)) {
    CHK_RELEASE(app_acc_obj);
    CHK_RELEASE(root_acc_obj);
    free(children);
    return hwnd;
  }

  // Get accessibility object for Chrome Client. If failed, return only
  // window handle.
  hr = GetChildrenArray(app_acc_obj, children);
  if (S_OK != hr) {
    CHK_RELEASE(app_acc_obj);
    CHK_RELEASE(root_acc_obj);
    free(children);
    return hwnd;
  }

  // Chrome Window has only one child which is Chrome Client.
  GetChildAccObject(app_acc_obj, children[CHROME_CLIENT_ACC_INDEX], acc_obj);

  // Done using [children] array.
  free(children);

  // Confirm if it is Chrome client using it's accessibility object's Name
  // and Role property. If it's not the desired object, return only window
  // handle.
  hr = (*acc_obj)->get_accName(id_self, &name);
  if ((S_OK != hr) || (!name) ||
      (0 != _wcsicmp(name, product_name.c_str())) ) {
    CHK_RELEASE(*acc_obj);
  }
  if (ROLE_SYSTEM_CLIENT != GetRole(*acc_obj))
    CHK_RELEASE(*acc_obj);

  CHK_RELEASE(app_acc_obj);
  CHK_RELEASE(root_acc_obj);
  return hwnd;
}

HRESULT GetChildAccessible(std::wstring parent_name, unsigned int child_index,
                           IAccessible** acc_obj) {
  // Validate input and initialize.
  if (!acc_obj)
    return E_INVALIDARG;

  *acc_obj = NULL;

  // Get accessibility object and window handle for Chrome parent.
  IAccessible* parent = NULL;
  if (0 == parent_name.compare(BROWSER_STR))
    GetChromeBrowserWnd(&parent);
  if (0 == parent_name.compare(BROWSER_VIEW_STR))
    GetBrowserViewAccessible(&parent);
  if (0 == parent_name.compare(TOOLBAR_STR))
    GetToolbarAccessible(&parent);
  if (0 == parent_name.compare(TABSTRIP_STR))
    GetTabStripAccessible(&parent);

  if (!parent)
    return E_FAIL;

  bool get_iaccessible = false;

  // Validate child index.
  INT64 child_cnt = GetChildCount(parent);
  if (child_index >= child_cnt)
    get_iaccessible = true;

  HRESULT hr = S_OK;

  if (get_iaccessible) {
    // Child retrieved by child index, potentially further down the hierarchy.
    VARIANT child_var;
    child_var.vt = VT_I4;
    child_var.lVal = child_index;
    hr = GetChildAccObject(parent, child_var, acc_obj);
  } else {
    // Get array of child items of parent object.
    VARIANT* children = reinterpret_cast<VARIANT*>(calloc(size_t(child_cnt),
                                                          sizeof(VARIANT)));
    if (children) {
      hr = GetChildrenArray(parent, children);
      if (S_OK == hr) {
        // Fetch child IAccessible.
        if (acc_obj)
          hr = GetChildAccObject(parent, children[child_index], acc_obj);
      }
      free(children);
    }
  }

  CHK_RELEASE(parent);
  return hr;
}

HRESULT GetTabStripAccessible(IAccessible** acc_obj) {
  return GetChildAccessible(BROWSER_VIEW_STR, TABSTRIP_ACC_INDEX, acc_obj);
}

HRESULT GetBrowserViewAccessible(IAccessible** acc_obj) {
  return GetChildAccessible(BROWSER_STR, BROWSER_VIEW_ACC_INDEX, acc_obj);
}

HRESULT GetToolbarAccessible(IAccessible** acc_obj) {
  return GetChildAccessible(BROWSER_VIEW_STR, VIEW_ID_TOOLBAR, acc_obj);
}

HRESULT GetBrowserMinimizeButton(IAccessible** acc_obj) {
  return GetChildAccessible(BROWSER_STR, CHROME_MIN_ACC_INDEX, acc_obj);
}

HRESULT GetBrowserMaximizeButton(IAccessible** acc_obj) {
  return GetChildAccessible(BROWSER_STR, CHROME_MAX_ACC_INDEX, acc_obj);
}

HRESULT GetBrowserRestoreButton(IAccessible** acc_obj) {
  return GetChildAccessible(BROWSER_STR, CHROME_RESTORE_ACC_INDEX, acc_obj);
}

HRESULT GetBrowserCloseButton(IAccessible** acc_obj) {
  return GetChildAccessible(BROWSER_STR, CHROME_CLOSE_ACC_INDEX, acc_obj);
}

HRESULT GetBackButton(IAccessible** acc_obj) {
  return GetChildAccessible(TOOLBAR_STR, VIEW_ID_BACK_BUTTON, acc_obj);
}

HRESULT GetForwardButton(IAccessible** acc_obj) {
  return GetChildAccessible(TOOLBAR_STR, VIEW_ID_FORWARD_BUTTON, acc_obj);
}

HRESULT GetStarButton(IAccessible** acc_obj) {
  return GetChildAccessible(TOOLBAR_STR, VIEW_ID_STAR_BUTTON, acc_obj);
}

HRESULT GetGoButton(IAccessible** acc_obj) {
  return GetChildAccessible(TOOLBAR_STR, VIEW_ID_GO_BUTTON, acc_obj);
}

HRESULT GetPageMenuButton(IAccessible** acc_obj) {
  return GetChildAccessible(TOOLBAR_STR, VIEW_ID_PAGE_MENU, acc_obj);
}

HRESULT GetAppMenuButton(IAccessible** acc_obj) {
  return GetChildAccessible(TOOLBAR_STR, VIEW_ID_APP_MENU, acc_obj);
}

HWND GetAddressBarWnd(IAccessible** acc_obj) {
  // Initialize, if requested.
  if (acc_obj)
    *acc_obj = NULL;

  HWND hwnd = NULL;
  HWND hwnd_addr_bar = NULL;

  // Get window handle for Chrome Browser.
  hwnd = GetChromeBrowserWnd(NULL);
  if (NULL != hwnd) {
    // Get AddressBar/OmniBox (edit box) window handle.
    hwnd_addr_bar = FindWindowEx(hwnd, 0, CHROME_AUTOCOMPLETE_EDIT, NULL);

    // Get accessibility object for address bar, if requested.
    if (acc_obj && hwnd_addr_bar) {
      AccessibleObjectFromWindow(hwnd_addr_bar, OBJID_WINDOW, IID_IAccessible,
                                 reinterpret_cast<void**>(acc_obj));
    }
  }

  return hwnd_addr_bar;
}

HWND GetFindTextWnd(IAccessible** acc_obj) {
  // Initialize, if requested.
  if (acc_obj)
    *acc_obj = NULL;

  HWND hwnd = NULL;
  HWND hwnd_find = NULL;

  // Get window handle for Chrome Browser.
  hwnd = GetChromeBrowserWnd(NULL);
  if (NULL != hwnd) {
    // Get handle of a window, which is contains edit box for Find string.
    hwnd_find = FindWindowEx(hwnd, 0, CHROME_HWND_VIEW_CONTAINER, NULL);

    // Get accessibility object, if requested.
    if (acc_obj && hwnd_find) {
      AccessibleObjectFromWindow(hwnd_find, OBJID_WINDOW, IID_IAccessible,
                                 reinterpret_cast<void**>(acc_obj));
    }
  }

  return hwnd_find;
}

HWND GetAuthWnd(IAccessible** acc_obj) {
  // Initialize, if requested.
  if (acc_obj)
    *acc_obj = NULL;

  HWND hwnd = NULL;
  HWND hwnd_tab = NULL;
  HWND hwnd_auth = NULL;

  // Get window handle for Chrome Browser.
  hwnd = GetChromeBrowserWnd(NULL);
  if (hwnd) {
    // Get window handle for tab.
    hwnd_tab = FindWindowEx(hwnd, 0, CHROME_TAB_CONTENTS, NULL);
    if (!hwnd_tab)
      return hwnd_auth;

    // Get handle for Authentication window.
    hwnd_auth = FindWindowEx(hwnd_tab, 0, CHROME_HWND_VIEW_CONTAINER,
                             AUTH_TITLE);

    // Get accessibility object, if requested.
    if (acc_obj && hwnd_auth) {
      AccessibleObjectFromWindow(hwnd_auth, OBJID_WINDOW, IID_IAccessible,
                                 reinterpret_cast<void**>(acc_obj));
    }
  }

  return hwnd_auth;
}

HRESULT GetChildAccObject(IAccessible* acc_obj, VARIANT child,
                          IAccessible** child_acc_obj) {
  // Validate input.
  if (!acc_obj || !child_acc_obj)
    return E_INVALIDARG;

  HRESULT hr = S_OK;
  IDispatch* disp = NULL;

  // Check the child type and fetch object accordingly.
  if (child.vt == VT_DISPATCH) {
    child.pdispVal->
        QueryInterface(IID_IAccessible,
                       reinterpret_cast<void**>(child_acc_obj));
  } else if (child.vt == VT_I4) {
    hr = acc_obj->get_accChild(child, &disp);
    if ((hr == S_OK) && disp) {
      disp->QueryInterface(IID_IAccessible,
                           reinterpret_cast<void**>(child_acc_obj));
      CHK_RELEASE(disp);
    }
  }

  return hr;
}

HRESULT GetParentAccObject(IAccessible* acc_obj, IAccessible** parent_acc_obj) {
  // Validate input.
  if (!acc_obj || !parent_acc_obj)
    return E_INVALIDARG;

  HRESULT hr = S_OK;
  IDispatch* disp = NULL;

  // Fetch parent object.
  hr = acc_obj->get_accParent(&disp);
  if ((hr == S_OK) && disp) {
    disp->QueryInterface(IID_IAccessible,
                         reinterpret_cast<void**>(parent_acc_obj));
    CHK_RELEASE(disp);
  }

  return hr;
}

INT64 GetChildCount(IAccessible* acc_obj) {
  HRESULT hr = S_OK;
  LONG child_cnt = 0;

  // Validate input. Object can have 0 children, so return -1 on invalid input.
  if (!acc_obj)
    return -1;

  // Get child count.
  acc_obj->get_accChildCount(&child_cnt);
  return child_cnt;
}

HRESULT GetChildrenArray(IAccessible* acc_obj, VARIANT* children) {
  HRESULT hr = S_OK;
  INT64 child_start = 0;
  LONG child_obtained = 0;
  INT64 child_cnt = GetChildCount(acc_obj);

  // Validate input.
  if (!acc_obj || !children)
    return E_INVALIDARG;

  // Validate every item and initialize it.
  int i = 0;
  for (; (i < child_cnt) && (children + i); i++) {
    VariantInit(children + i);
  }

  // If all items in array are not initialized, return error.
  if (i != child_cnt)
    return E_FAIL;

  // Get IDs of child items.
  AccessibleChildren(acc_obj, LONG(child_start), LONG(child_cnt), children,
                     &child_obtained);
  return hr;
}

HRESULT ActivateWnd(IAccessible* acc_obj, HWND hwnd) {
  HRESULT hr = S_OK;

  // Select and focus the object, if accessibility object is specified.
  if (acc_obj)
    hr = acc_obj->accSelect(SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION, id_self);

  // Send message to window, if window handle is specified.
  if (hwnd)
    SetActiveWindow(hwnd);

  return hr;
}

BSTR GetTabName(INT64 tab_index) {
  HRESULT hr = S_OK;
  BSTR name;

  // Validate tab index specified.
  if (tab_index < 1)
    return NULL;

  // Get accessibility object for Tabstrip.
  IAccessible* tab_strip_acc_obj = NULL;
  GetTabStripAccessible(&tab_strip_acc_obj);

  // Get Tab from Tabstrip and return it's Name.
  if (tab_strip_acc_obj) {
    INT64 child_cnt = GetChildCount(tab_strip_acc_obj);
    VARIANT* children = reinterpret_cast<VARIANT*>(calloc(size_t(child_cnt),
                                                          sizeof(VARIANT)));
    if (children) {
      // Get tab object. tab_index = index in child array, because first child
      // in tabstrip is '+' button.
      hr = GetChildrenArray(tab_strip_acc_obj, children);
      if (S_OK == hr) {
        IAccessible* temp_acc_obj = NULL;
        hr = GetChildAccObject(tab_strip_acc_obj, children[tab_index],
                            &temp_acc_obj);
        if ((S_OK == hr) && (children[tab_index].vt == VT_DISPATCH) &&
            (temp_acc_obj)) {
          hr = temp_acc_obj->get_accName(id_self, &name);
        } else if (children[tab_index].vt == VT_I4) {
          hr = tab_strip_acc_obj->get_accName(children[1], &name);
        }
        CHK_RELEASE(temp_acc_obj);
        CHK_RELEASE(tab_strip_acc_obj);
        return name;
      }
    }
    CHK_RELEASE(tab_strip_acc_obj);
  }

  return NULL;
}

INT64 GetTabCnt() {
  // Get accessibility object for Tabstrip.
  IAccessible* tab_strip_acc_obj = NULL;
  GetTabStripAccessible(&tab_strip_acc_obj);

  // If Tabstrip is invalid, return -1 to indicate error.
  if (!tab_strip_acc_obj)
    return -1;

  // Get child count.
  INT64 child_cnt = 0;
  child_cnt = GetChildCount(tab_strip_acc_obj);
  CHK_RELEASE(tab_strip_acc_obj);

  // Don't count 1st child as it is '+' button.
  return (child_cnt - 1);
}

std::wstring GetName(IAccessible* acc_obj, VARIANT child) {
  HRESULT hr = S_OK;

  // Validate input.
  if (!acc_obj)
    return std::wstring();

  // Get Name.
  BSTR name;
  hr = acc_obj->get_accName(child, &name);
  if (S_OK != hr)
    return std::wstring();

  return std::wstring(name);
}

LONG GetRole(IAccessible* acc_obj, VARIANT child) {
  HRESULT hr = S_OK;
  LPTSTR role_str = NULL;

  // Validate input.
  if (!acc_obj)
    return -1;

  // Get Role.
  VARIANT role;
  VariantInit(&role);
  hr = acc_obj->get_accRole(child, &role);
  if (S_OK != hr || VT_I4 != role.vt) {
    VariantClear(&role);
    return -1;
  }

  // Return the role value
  return role.lVal;
}

LONG GetState(IAccessible* acc_obj, VARIANT child) {
  HRESULT hr = S_OK;
  LPTSTR state_str = NULL;
  std::wstring complete_state;

  // Validate input.
  if (!acc_obj)
    return -1;

  // Get State.
  VARIANT state;
  VariantInit(&state);
  hr = acc_obj->get_accState(child, &state);
  if (S_OK != hr || VT_I4 != state.vt) {
    VariantClear(&state);
    return -1;
  }

  VariantClear(&state);
  return state.lVal;
}
