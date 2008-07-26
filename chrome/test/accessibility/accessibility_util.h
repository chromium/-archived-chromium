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

#ifndef CHROME_TEST_ACCISSIBILITY_ACCISSIBILITY_UTIL_H__
#define CHROME_TEST_ACCISSIBILITY_ACCISSIBILITY_UTIL_H__

#include <Oleacc.h>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
// Functions and Globals which are using IAccessible interface.
// These are the wrappers to fetch accessible object interface and properties.
///////////////////////////////////////////////////////////////////////////////

// Variant ID pointing to object itself.
extern VARIANT g_var_self;

// Returns window handle to Chrome Browser. Retrives it's(having role as
// client) IAccessible pointer, if requested.
HWND GetChromeBrowserWnd(IAccessible** ppi_access);

// Returns IAccessible pointer of object's child window, provided parent's name
// and it's 0 based child index. If child is element and not complete object,
// it's variant id is returned with S_FALSE.
HRESULT GetChildWndOf(std::wstring parent_name, unsigned int child_index,
                      IAccessible** ppi_access, VARIANT* child_var_id);

// Returns IAccessible pointer for Tabstrip. It does not have window handle.
HRESULT GetTabStripWnd(IAccessible** ppi_access);

// Returns IAccessible pointer for Toolbar. It does not have window handle.
HRESULT GetToolbarWnd(IAccessible** ppi_access);

// Returns handle to OmniBox(AddressBar) and IAccessible pointer, if requested.
HWND GetAddressBarWnd(IAccessible** ppi_access);

// Returns handle to Find box and IAccessible pointer, if requested.
HWND GetFindTextWnd(IAccessible** ppi_access);

// Returns handle to authentication dialog and IAccessible pointer, if
// requested.
HWND GetAuthWnd(IAccessible** ppi_access);

// Fetches IAccessible pointer for a child of given the IAccessible pointer
// and desired child id.
HRESULT GetChildObject(IAccessible* pi_access, VARIANT var_child,
                       IAccessible** ppi_child_access);

// Fetches IAccessible pointer for a parent of specified IAccessible pointer.
HRESULT GetParentObject(IAccessible* pi_access,
                        IAccessible** ppi_parent_access);

// Returns no. of child items of specified IAccessible pointer. If input
// parameter is NULL, -1 is returned.
INT64 GetChildCount(IAccessible* pi_access);

// Extracts (VARIANT)array of child items of specified IAccessible pointer.
HRESULT GetChildrenArray(IAccessible* pi_access, VARIANT* var_array_child);

// Activates specified window using IAccessible pointer and/or window handle.
HRESULT ActivateWnd(IAccessible *pi_access, HWND hwnd);

// Returns title of tab whose index is specified. Tab index starts from 1.
BSTR GetTabName(INT64 tab_index);

// Returns no. of tabs in tabstrip. If processing fails, it returns -1.
INT64 GetTabCnt();

// Returns Name of specified IAccessible pointer or it's child specified by
// variant.
std::wstring GetName(IAccessible* pi_access, VARIANT child = g_var_self);

// Returns Role of specified IAccessible pointer or it's child specified by
// variant.
std::wstring GetRole(IAccessible* pi_access, VARIANT child = g_var_self);

// Returns State of specified IAccessible pointer or it's child specified by
// variant.
std::wstring GetState(IAccessible* pi_access, VARIANT child = g_var_self);

// Returns IAccessible pointer for Chrome Minimize Button. It does not have
// window handle.
HRESULT GetBrowserMinimizeButton(IAccessible** ppi_access,
                                 VARIANT* child_var_id);

// Returns IAccessible pointer for Chrome Maximize Button. It does not have
// window handle.
HRESULT GetBrowserMaximizeButton(IAccessible** ppi_access,
                                 VARIANT* child_var_id);

// Returns IAccessible pointer for Chrome Restore Button. It does not have
// window handle.
HRESULT GetBrowserRestoreButton(IAccessible** ppi_access,
                                VARIANT* child_var_id);

// Returns IAccessible pointer for Chrome Close Button. It does not have
// window handle.
HRESULT GetBrowserCloseButton(IAccessible** ppi_access, VARIANT* child_var_id);

// Returns IAccessible pointer for Star Button. It does not have window handle.
HRESULT GetStarButton(IAccessible** ppi_access, VARIANT* child_var_id);

// Returns IAccessible pointer for Back Button. It does not have window handle.
HRESULT GetBackButton(IAccessible** ppi_access, VARIANT* child_var_id);

// Returns IAccessible pointer for Forward Button. It does not have window
// handle.
HRESULT GetForwardButton(IAccessible** ppi_access, VARIANT* child_var_id);

#endif  // CHROME_TEST_ACCISSIBILITY_ACCISSIBILITY_UTIL_H__
