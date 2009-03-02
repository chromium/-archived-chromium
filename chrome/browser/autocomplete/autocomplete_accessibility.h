// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_ACCESSIBILITY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_ACCESSIBILITY_H_

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include "base/basictypes.h"

class AutocompleteEditViewWin;

////////////////////////////////////////////////////////////////////////////////
//
// AutocompleteAccessibility
//
// Class implementing the MSAA IAccessible COM interface for
// AutocompleteEditViewWin, providing accessibility to be used by screen
// readers and other assistive technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class ATL_NO_VTABLE AutocompleteAccessibility
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchImpl<IAccessible, &IID_IAccessible, &LIBID_Accessibility> {
 public:
  BEGIN_COM_MAP(AutocompleteAccessibility)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible)
    COM_INTERFACE_ENTRY(IAccessible)
  END_COM_MAP()

  AutocompleteAccessibility() {}
  ~AutocompleteAccessibility() {}

  HRESULT Initialize(const AutocompleteEditViewWin* edit_box);

  // Supported IAccessible methods.

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count);

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child);

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent);

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end);

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child);

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name);

  // Retrieves the tooltip description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc);

  // Returns the current value of the edit box.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value);

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state);

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role);

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id, BSTR* default_action);

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left, LONG* y_top, LONG* width, LONG* height,
                           VARIANT var_id);

  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child);

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id, BSTR* access_key);

  // Non-supported IAccessible methods.

  // Out-dated and can be safely said to be very rarely used.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id);

  // Selections not applicable to views.
  STDMETHODIMP get_accSelection(VARIANT* selected);
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id);

  // Help functions not supported.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help);
  STDMETHODIMP get_accHelpTopic(BSTR* help_file, VARIANT var_id,
                                LONG* topic_id);

  // Deprecated functions, not implemented here.
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name);
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR put_val);

 protected:
  // A pointer containing the Windows' default IAccessible implementation for
  // this object. Used where it is acceptable to return default MSAA
  // information.
  CComPtr<IAccessible> default_accessibility_server_;

 private:
  const AutocompleteEditViewWin* edit_box_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteAccessibility);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_ACCESSIBILITY_H_
