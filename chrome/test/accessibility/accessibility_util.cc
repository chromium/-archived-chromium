// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/accessibility_util.h"

#include "base/win_util.h"
#include "chrome/common/win_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/browser/views/old_frames/xp_frame.h"
#include "chrome/browser/views/old_frames/vista_frame.h"
#include "chrome/test/accessibility/constants.h"

#include "chromium_strings.h"
#include "generated_resources.h"

VARIANT g_var_self = {VT_I4, CHILDID_SELF};

// TODO(beng): clean this up
static const wchar_t* kBrowserWindowKey = L"__BROWSER_WINDOW__";

static BOOL CALLBACK WindowEnumProc(HWND hwnd, LPARAM data) {
  std::wstring class_name = win_util::GetClassName(hwnd);
  if (class_name == L"Chrome_HWNDViewContainer_0") {
    HANDLE window_interface = GetProp(hwnd, kBrowserWindowKey);
    if (window_interface) {
      HWND* out = reinterpret_cast<HWND*>(data);
      *out = hwnd;
      return FALSE;
    }
  }
  return TRUE;
}

HWND GetChromeBrowserWnd(IAccessible** ppi_access) {
  HRESULT      hr   = S_OK;
  HWND         hwnd = NULL;
  BSTR         str_name;
  std::wstring str_role;

  const std::wstring product_name = l10n_util::GetString(IDS_PRODUCT_NAME);

  EnumWindows(WindowEnumProc, reinterpret_cast<LPARAM>(&hwnd));
  if (!IsWindow(hwnd)) {
    // Didn't find the window handle by looking for the new frames, assume the
    // old frames are being used instead...
    if (win_util::ShouldUseVistaFrame()) {
      hwnd = FindWindow(VISTA_FRAME_CLASSNAME, NULL);
    } else {
      hwnd = FindWindow(XP_FRAME_CLASSNAME, NULL);
    }
  }

  if (NULL == hwnd) {
    if (ppi_access)
      *ppi_access = NULL;
    return hwnd;
  }

  // Get accessibility object for Chrome, only if requested.
  if (!ppi_access) {
    return hwnd;
  }
  *ppi_access = NULL;

  // Get accessibility object for Chrome Main Window. If failed to get it,
  // return only window handle.
  IAccessible *pi_acc_root_win = NULL;
  hr = AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                  reinterpret_cast<void**>
                                  (&pi_acc_root_win));
  if ((S_OK != hr) || !pi_acc_root_win) {
    return hwnd;
  }


  // Confirm if it is Chrome window using it's accessibility object's
  // Name and Role property. If it's not the desired object, return only
  // window handle.
  hr = pi_acc_root_win->get_accName(g_var_self, &str_name);
  if ((S_OK != hr) || (!str_name) ||
      (0 != _wcsicmp(str_name, product_name.c_str())) ) {
    CHK_RELEASE(pi_acc_root_win);
    return hwnd;
  }
  str_role = GetRole(pi_acc_root_win);
  if (0 != str_role.compare(BROWSER_WIN_ROLE)) {
    CHK_RELEASE(pi_acc_root_win);
    return hwnd;
  }

  // Get accessibility object for Chrome Window. If failed to get it,
  // return only window handle.
  INT64 child_cnt = GetChildCount(pi_acc_root_win);
  VARIANT *var_array_child =
      reinterpret_cast<VARIANT*>(calloc(size_t(child_cnt),
                                     sizeof(VARIANT)));

  if (!var_array_child) {
    CHK_RELEASE(pi_acc_root_win);
    return hwnd;
  }

  hr = GetChildrenArray(pi_acc_root_win, var_array_child);
  if (S_OK != hr) {
    CHK_RELEASE(pi_acc_root_win);
    return hwnd;
  }

  // Fetch desired child (Chrome window) of Chrome Main Window.
  IAccessible *pi_acc_app = NULL;
  GetChildObject(pi_acc_root_win, var_array_child[CHROME_APP_ACC_INDEX],
                 &pi_acc_app);
  if (!pi_acc_app) {
    CHK_RELEASE(pi_acc_app);
    return hwnd;
  }

  // Confirm if it is Chrome application using it's accessibility object's
  // Name and Role property. If it's not the desired object, return only
  // window handle.
  hr = pi_acc_app->get_accName(g_var_self, &str_name);
  if ((S_OK != hr) || (!str_name) ||
      (0 != _wcsicmp(str_name, product_name.c_str())) ) {
    CHK_RELEASE(pi_acc_app);
    CHK_RELEASE(pi_acc_root_win);
    return hwnd;
  }
  str_role = GetRole(pi_acc_app);
  if (0 != str_role.compare(BROWSER_APP_ROLE)) {
    CHK_RELEASE(pi_acc_app);
    CHK_RELEASE(pi_acc_root_win);
    return hwnd;
  }

  // Get accessibility object for Chrome Client. If failed, return only
  // window handle.
  hr = GetChildrenArray(pi_acc_app, var_array_child);
    if (S_OK != hr) {
      CHK_RELEASE(pi_acc_app);
      CHK_RELEASE(pi_acc_root_win);
      return hwnd;
    }

  // Chrome Window has only one child which is Chrome Client.
  GetChildObject(pi_acc_app, var_array_child[CHROME_CLIENT_ACC_INDEX],
                 ppi_access);

  // Confirm if it is Chrome client using it's accessibility object's Name
  // and Role property. If it's not the desired object, return only window
  // handle.
  hr = (*ppi_access)->get_accName(g_var_self, &str_name);
  if ((S_OK != hr) || (!str_name) ||
      (0 != _wcsicmp(str_name, product_name.c_str())) ) {
    CHK_RELEASE(*ppi_access);
  }
  str_role = GetRole(*ppi_access);
  if (0 != str_role.compare(BROWSER_CLIENT_ROLE)) {
    CHK_RELEASE(*ppi_access);
  }

  CHK_RELEASE(pi_acc_app);
  CHK_RELEASE(pi_acc_root_win);
  return hwnd;
}

HRESULT GetChildWndOf(std::wstring parent_name, unsigned int child_index,
                      IAccessible** ppi_access, VARIANT* child_var_id) {
  HRESULT hr = S_OK;

  // Validate input and initialize.
  if (!ppi_access && !child_var_id)
    return E_INVALIDARG;
  if (ppi_access)
    *ppi_access = NULL;
  if (child_var_id)
    VariantInit(child_var_id);

  // Get accessibility object and window handle for Chrome parent.
  IAccessible *pi_parent = NULL;
  if (0 == parent_name.compare(BROWSER_STR))
    GetChromeBrowserWnd(&pi_parent);
  if (0 == parent_name.compare(BROWSER_VIEW_STR))
    GetBrowserViewWnd(&pi_parent);
  if (0 == parent_name.compare(TOOLBAR_STR))
    GetToolbarWnd(&pi_parent);
  if (0 == parent_name.compare(TABSTRIP_STR))
    GetTabStripWnd(&pi_parent);

  if (!pi_parent)
    return E_FAIL;

  // Validate child index.
  INT64 child_cnt = GetChildCount(pi_parent);
  if (child_index >= child_cnt) {
    CHK_RELEASE(pi_parent);
    VariantClear(child_var_id);
    return E_INVALIDARG;
  }

  // Get array of child items of parent object.
  VARIANT *var_array_child =
      reinterpret_cast<VARIANT*>(calloc(size_t(child_cnt), sizeof(VARIANT)));
  if (var_array_child) {
    hr = GetChildrenArray(pi_parent, var_array_child);
    if (S_OK == hr) {
      // Fetch Tabstrip which is child_index'th child of parent object.
      if (ppi_access) {
        hr = GetChildObject(pi_parent, var_array_child[child_index],
                            ppi_access);
      }
      if (child_var_id) {
        VariantCopy(child_var_id, var_array_child+child_index);
      }
    }
    free(var_array_child);
  }

  CHK_RELEASE(pi_parent);
  return hr;
}

HRESULT GetTabStripWnd(IAccessible** ppi_access) {
#ifdef NEW_FRAMES
  return GetChildWndOf(BROWSER_VIEW_STR, TABSTRIP_ACC_INDEX, ppi_access, NULL);
#else
  return GetChildWndOf(BROWSER_STR, TABSTRIP_ACC_INDEX, ppi_access, NULL);
#endif
}

HRESULT GetBrowserViewWnd(IAccessible** ppi_access) {
  return GetChildWndOf(BROWSER_STR, BROWSER_VIEW_ACC_INDEX, ppi_access, NULL);
}

HRESULT GetToolbarWnd(IAccessible** ppi_access) {
  return GetChildWndOf(BROWSER_VIEW_STR, TOOLBAR_ACC_INDEX, ppi_access, NULL);
}

HRESULT GetBrowserMinimizeButton(IAccessible** ppi_access,
                                 VARIANT* child_var_id) {
  return GetChildWndOf(BROWSER_STR, CHROME_MIN_ACC_INDEX, ppi_access,
                       child_var_id);
}

HRESULT GetBrowserMaximizeButton(IAccessible** ppi_access,
                                 VARIANT* child_var_id) {
  return GetChildWndOf(BROWSER_STR, CHROME_MAX_ACC_INDEX, ppi_access,
                       child_var_id);
}

HRESULT GetBrowserRestoreButton(IAccessible** ppi_access,
                                VARIANT* child_var_id) {
  return GetChildWndOf(BROWSER_STR, CHROME_RESTORE_ACC_INDEX, ppi_access,
                       child_var_id);
}

HRESULT GetBrowserCloseButton(IAccessible** ppi_access,
                              VARIANT* child_var_id) {
  return GetChildWndOf(BROWSER_STR, CHROME_CLOSE_ACC_INDEX, ppi_access,
                       child_var_id);
}

HRESULT GetStarButton(IAccessible** ppi_access, VARIANT* child_var_id) {
  return GetChildWndOf(TOOLBAR_STR, STAR_BTN_INDEX, ppi_access, child_var_id);
}

HRESULT GetBackButton(IAccessible** ppi_access, VARIANT* child_var_id) {
  return GetChildWndOf(TOOLBAR_STR, BACK_BTN_INDEX, ppi_access, child_var_id);
}

HRESULT GetForwardButton(IAccessible** ppi_access, VARIANT* child_var_id) {
  return GetChildWndOf(TOOLBAR_STR, FORWARD_BTN_INDEX, ppi_access,
                       child_var_id);
}

HWND GetAddressBarWnd(IAccessible** ppi_access) {
  HWND    hwnd          = NULL;
  HWND    hwnd_addr_bar = NULL;

  // // Initialize, if requested.
  if (ppi_access) {
    *ppi_access = NULL;
  }

  // Get window handle for Chrome Browser.
  hwnd = GetChromeBrowserWnd(NULL);
  if (NULL != hwnd) {
    // Get AddressBar/OmniBox (edit box) window handle.
    hwnd_addr_bar = FindWindowEx(hwnd, 0, CHROME_AUTOCOMPLETE_EDIT, NULL);

    // Get accessibility object for address bar, if requested.
    if (ppi_access && hwnd_addr_bar) {
      AccessibleObjectFromWindow(hwnd_addr_bar, OBJID_WINDOW, IID_IAccessible,
                                 reinterpret_cast<void**>(ppi_access));
    }
  }

  return hwnd_addr_bar;
}

HWND GetFindTextWnd(IAccessible** ppi_access) {
  HWND    hwnd      = NULL;
  HWND    hwnd_find = NULL;

  // Initialize, if requested.
  if (ppi_access) {
    *ppi_access = NULL;
  }

  // Get window handle for Chrome Browser.
  hwnd = GetChromeBrowserWnd(NULL);
  if (NULL != hwnd) {
    // Get handle of a window, which is contains edit box for Find string.
    hwnd_find = FindWindowEx(hwnd, 0, CHROME_HWND_VIEW_CONTAINER, NULL);

    // Get accessibility object, if requested.
    if (ppi_access && hwnd_find) {
      AccessibleObjectFromWindow(hwnd_find, OBJID_WINDOW, IID_IAccessible,
                                 reinterpret_cast<void**>(ppi_access));
    }
  }

  return hwnd_find;
}

HWND GetAuthWnd(IAccessible** ppi_access) {
  HWND    hwnd      = NULL;
  HWND    hwnd_tab  = NULL;
  HWND    hwnd_auth = NULL;

  // Initialize, if requested.
  if (ppi_access) {
    *ppi_access = NULL;
  }

  // Get window handle for Chrome Browser.
  hwnd = GetChromeBrowserWnd(NULL);
  if (NULL != hwnd) {
    // Get window handle for tab.
    hwnd_tab = FindWindowEx(hwnd, 0, CHROME_TAB_CONTENTS, NULL);
    if (!hwnd_tab)
      return hwnd_auth;

    // Get handle for Authentication window.
    hwnd_auth = FindWindowEx(hwnd_tab, 0, CHROME_HWND_VIEW_CONTAINER,
                             AUTH_TITLE);

    // Get accessibility object, if requested.
    if (ppi_access && hwnd_auth) {
      AccessibleObjectFromWindow(hwnd_auth, OBJID_WINDOW, IID_IAccessible,
                                 reinterpret_cast<void**>(ppi_access));
    }
  }

  return hwnd_auth;
}

HRESULT GetChildObject(IAccessible* pi_access, VARIANT var_child,
                       IAccessible** ppi_child_access) {
  HRESULT   hr          = S_OK;
  IDispatch *p_dispatch = NULL;

  // Validate input.
  if ( (pi_access == NULL) ||
       (ppi_child_access == NULL) ) {
    return E_INVALIDARG;
  }

  // Check the child type and fetch object accordingly.
  if (var_child.vt == VT_DISPATCH) {
    var_child.pdispVal->
    QueryInterface(IID_IAccessible,
                   reinterpret_cast<void**>(ppi_child_access));
  } else if (var_child.vt == VT_I4) {
    hr = pi_access->get_accChild(var_child, &p_dispatch);
    if ( (hr == S_OK) &&
         (p_dispatch != NULL) ) {
      p_dispatch->QueryInterface(IID_IAccessible,
                                 reinterpret_cast<void**>(ppi_child_access));
      CHK_RELEASE(p_dispatch);
    }
  }

  return hr;
}

HRESULT GetParentObject(IAccessible* pi_access,
                        IAccessible** ppi_parent_access) {
  HRESULT   hr          = S_OK;
  IDispatch *p_dispatch = NULL;

  // Validate input.
  if ( (pi_access == NULL) ||
       (ppi_parent_access == NULL) ) {
    return E_INVALIDARG;
  }

  // Fetch parent object.
  hr = pi_access->get_accParent(&p_dispatch);
  if ( (hr == S_OK) &&
       (p_dispatch != NULL) ) {
    p_dispatch->QueryInterface(IID_IAccessible,
                               reinterpret_cast<void**>(ppi_parent_access));
    CHK_RELEASE(p_dispatch);
  }

  return hr;
}

INT64 GetChildCount(IAccessible* pi_access) {
  HRESULT hr        = S_OK;
  long    child_cnt = 0;

  // Validate input. Object can have 0 children. So return -1 on invalid input.
  if (pi_access == NULL) {
    return -1;
  }

  // Get child count.
  pi_access->get_accChildCount(&child_cnt);
  return child_cnt;
}

HRESULT GetChildrenArray(IAccessible* pi_access, VARIANT* var_array_child) {
  HRESULT hr              = S_OK;
  INT64    child_start    = 0;
  long     child_obtained = 0;
  INT64    child_cnt      = GetChildCount(pi_access);

  // Validate input.
  if ((pi_access == NULL) || (var_array_child == NULL)) {
    return E_INVALIDARG;
  }

  // Validate every item and initialize it.
  int i = 0;
  for (; (i < child_cnt) && (var_array_child+i); i++) {
    VariantInit(var_array_child+i);
  }

  // If all items in array are not initialized, return error.
  if (i != child_cnt) {
    return E_INVALIDARG;
  }

  // Get IDs of child items.
  AccessibleChildren(pi_access,
                     LONG(child_start),
                     LONG(child_cnt),
                     var_array_child,
                     &child_obtained);
  return hr;
}

HRESULT ActivateWnd(IAccessible *pi_access, HWND hwnd) {
  HRESULT hr = S_OK;

  // Select and focus the object, if accessibility object is specified.
  if (pi_access) {
    hr = pi_access->accSelect(SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION,
                              g_var_self);
  }

  // Send message to window, if window handle is specified.
  if (hwnd) {
    SetActiveWindow(hwnd);
  }

  return hr;
}

BSTR GetTabName(INT64 tab_index) {
  HRESULT hr = S_OK;
  BSTR    str_name;

  // Validate tab index specified.
  if (tab_index < 1)
    return NULL;

  // Get accessibility object for Tabstrip.
  IAccessible *pi_acc_strip = NULL;
  GetTabStripWnd(&pi_acc_strip);

  // Get Tab from Tabstrip and return it's Name.
  if (pi_acc_strip) {
    INT64   child_cnt = GetChildCount(pi_acc_strip);
    VARIANT *var_array_child =
        reinterpret_cast<VARIANT*>(calloc(size_t(child_cnt), sizeof(VARIANT)));
    if (var_array_child) {
      // Get tab object. tab_index = index in child array, because first child
      // in tabstrip is '+' button.
      hr = GetChildrenArray(pi_acc_strip, var_array_child);
      if (S_OK == hr) {
        IAccessible *pi_access_temp = NULL;
        hr = GetChildObject(pi_acc_strip, var_array_child[tab_index],
                            &pi_access_temp);
        if ((S_OK == hr) &&
            (var_array_child[tab_index].vt == VT_DISPATCH) &&
            (pi_access_temp) ) {
          hr = pi_access_temp->get_accName(g_var_self, &str_name);
        } else if (var_array_child[tab_index].vt == VT_I4) {
          hr = pi_acc_strip->get_accName(var_array_child[1], &str_name);
        }
        CHK_RELEASE(pi_acc_strip);
        return str_name;
      }
    }

    CHK_RELEASE(pi_acc_strip);
  }

  return NULL;
}

INT64 GetTabCnt() {
  // Get accessibility object for Tabstrip.
  IAccessible *pi_acc_strip = NULL;
  GetTabStripWnd(&pi_acc_strip);

  // If Tabstrip is invalid, return -1, to indicate error.
  if (!pi_acc_strip) {
    return -1;
  }

  // Get child count.
  INT64   child_cnt = 0;
  if (pi_acc_strip) {
    child_cnt = GetChildCount(pi_acc_strip);
    CHK_RELEASE(pi_acc_strip);
  }

  // Don't count 1st child as it is '+' button.
  return (child_cnt-1);
}

std::wstring GetName(IAccessible* pi_access, VARIANT child) {
  HRESULT hr = S_OK;

  // Validate input.
  if (NULL == pi_access) {
    return std::wstring();
  }

  // Get Name.
  BSTR name;
  hr = pi_access->get_accName(child, &name);
  if (S_OK != hr)
    return std::wstring();

  return std::wstring(name);
}

std::wstring GetRole(IAccessible* pi_access, VARIANT child) {
  HRESULT hr       = S_OK;
  LPTSTR  role_str = NULL;

  // Validate input.
  if (NULL == pi_access) {
    return std::wstring();
  }

  // Get Role.
  VARIANT role;
  VariantInit(&role);
  hr = pi_access->get_accRole(child, &role);
  if (S_OK != hr || VT_I4 != role.vt) {
    VariantClear(&role);
    return std::wstring();
  }

  // Get Role string.
  unsigned int role_length = GetRoleText(role.lVal, NULL, 0);
  role_str = (LPTSTR)calloc(role_length + 1, sizeof(TCHAR));
  if (role_str)
    GetRoleText(role.lVal, role_str, role_length + 1);

  VariantClear(&role);
  return std::wstring(role_str);
}

std::wstring GetState(IAccessible* pi_access, VARIANT child) {
  HRESULT      hr        = S_OK;
  LPTSTR       state_str = NULL;
  std::wstring complete_state;

  // Validate input.
  if (NULL == pi_access) {
    return std::wstring();
  }

  // Get State.
  VARIANT state;
  VariantInit(&state);
  hr = pi_access->get_accState(child, &state);
  if (S_OK != hr || VT_I4 != state.vt) {
    VariantClear(&state);
    return std::wstring();
  }

  // Treat the "normal" state separately.
  if (state.vt == 0) {
    unsigned int state_length = GetStateText(state.lVal, NULL, 0);
    state_str = (LPTSTR)calloc(state_length + 1, sizeof(TCHAR));
    if (state_str) {
      GetStateText(state.lVal, state_str, state_length + 1);
      complete_state = std::wstring(state_str);
    }
  } else {
    // Number of bits.
    UINT bit_cnt = 32;
    // Convert state flags to comma separated list.
    for (DWORD dwStateBit = 0x80000000; bit_cnt; bit_cnt--, dwStateBit >>= 1) {
      if (state.lVal & dwStateBit) {
        unsigned int state_length = GetStateText(dwStateBit, NULL, 0);
        state_str = (LPTSTR)calloc(state_length + 1, sizeof(TCHAR));
        if (state_str) {
          GetStateText(dwStateBit, state_str, state_length + 1);
          if (complete_state.length() > 0)
            complete_state.append(L", ");
          complete_state.append(state_str);
          free(state_str);
        }
      }
    }
  }

  VariantClear(&state);
  return complete_state;
}

