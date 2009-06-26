// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
#define CHROME_TEST_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_

#include <oleacc.h>

#include "base/string_util.h"

///////////////////////////////////////////////////////////////////////////////
// Functions and Globals that use the IAccessible interface.
// These are the wrappers to fetch accessible object interface and properties.
///////////////////////////////////////////////////////////////////////////////

// Variant ID pointing to object itself.
extern VARIANT id_self;

// Returns window handle to Chrome Browser, along with (if requested) its
// IAccessible implementation, by calling AccessibleObjectFromWindow on the
// window handle. The IAccessible hierarchy (root->app->client) is also verified
// in terms of accessible name and role. If [acc_obj] is NULL, only the window
// handle is returned.
HWND GetChromeBrowserWnd(IAccessible** acc_obj);

// Returns IAccessible pointer of object's child window, provided [parent_name]
// and its (0-based) [child_index]. If child is a leaf element (has no children)
// its variant id is returned with S_FALSE.
HRESULT GetChildAccessible(std::wstring parent_name, unsigned int child_index,
                           IAccessible** acc_obj);

// Returns IAccessible pointer for Tabstrip (does not have a window handle), by
// calling upon GetChildAccessible. Will never be a leaf element, as it always
// has at least one child.
HRESULT GetTabStripAccessible(IAccessible** acc_obj);

// Returns IAccessible pointer for BrowserView (does not have a window handle),
// by calling upon GetChildAccessible. Will never be a leaf element, as it
// always has at least one child.
HRESULT GetBrowserViewAccessible(IAccessible** acc_obj);

// Returns IAccessible pointer for Toolbar (does not have a window handle), by
// calling upon GetChildAccessible. Will never be a leaf element, as it always
// has at least one child.
HRESULT GetToolbarAccessible(IAccessible** acc_obj);

// Returns window handle to OmniBox(AddressBar) and IAccessible pointer (if
// requested), by calling AccessibleObjectFromWindow on the window handle. If
// [acc_obj] is NULL, only the window handle is returned.
HWND GetAddressBarWnd(IAccessible** acc_obj);

// Returns window handle to Find edit box and IAccessible pointer (if
// requested), by calling AccessibleObjectFromWindow on the window handle.  If
// [acc_obj] is NULL, only the window handle is returned.
HWND GetFindTextWnd(IAccessible** acc_obj);

// Returns window handle to Authentication dialog and IAccessible pointer (if
// requested), by calling AccessibleObjectFromWindow on the window handle.  If
// [acc_obj] is NULL, only the window handle is returned.
HWND GetAuthWnd(IAccessible** acc_obj);

// Fetches IAccessible pointer for a child, given the IAccessible for the parent
// ([acc_obj]) and a child id (passed in with the [child] VARIANT). Retrieves
// the child by calling get_accChild on [acc_obj].
HRESULT GetChildAccObject(IAccessible* acc_obj, VARIANT child,
                          IAccessible** child_acc_obj);

// Fetches IAccessible pointer for the parent of specified IAccessible object
// (by calling get_accParent on [acc_obj]).
HRESULT GetParentAccObject(IAccessible* acc_obj, IAccessible** parent_acc_obj);

// Returns number of children for the specified IAccessible. If [acc_obj]
// parameter is NULL, -1 is returned.
INT64 GetChildCount(IAccessible* acc_obj);

// Extracts (VARIANT)array of child items of specified IAccessible pointer,
// by calling the AccessibleChildren function in MSAA.
HRESULT GetChildrenArray(IAccessible* acc_obj, VARIANT* children);

// Activates specified window using IAccessible pointer and/or window handle.
// Also calls accSelect on [acc_obj] to set accessibility focus and selection.
HRESULT ActivateWnd(IAccessible* acc_obj, HWND hwnd);

// Returns title of tab whose index is specified. Tab index starts from 1.
BSTR GetTabName(INT64 tab_index);

// Returns number of tabs in tabstrip. If processing fails, it returns -1.
INT64 GetTabCnt();

// Returns Name of specified [acc_obj] or its [child], by calling get_accName.
// If input is invalid, an empty std::wstring is returned.
std::wstring GetName(IAccessible* acc_obj, VARIANT child = id_self);

// Returns Role of specified [acc_obj] or its [child], by calling get_accRole. A
// returned value of -1 indicates error.
LONG GetRole(IAccessible* acc_obj, VARIANT child = id_self);

// Returns State of specified [acc_obj] or its [child], by calling get_accState.
// A returned value of -1 indicates error.
LONG GetState(IAccessible* acc_obj, VARIANT child = id_self);

// Returns IAccessible pointer for Chrome Minimize Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetBrowserMinimizeButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Maximize Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetBrowserMaximizeButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Restore Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetBrowserRestoreButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Close Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetBrowserCloseButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Back Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetBackButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Forward Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetForwardButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Star Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetStarButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Go Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetGoButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome Page Menu Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetPageMenuButton(IAccessible** acc_obj);

// Returns IAccessible pointer for Chrome App Menu Button, by calling
// GetChildAccessible. It does not have window handle.
HRESULT GetAppMenuButton(IAccessible** acc_obj);

#endif  // CHROME_TEST_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
