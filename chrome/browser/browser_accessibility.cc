// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility.h"

#include "base/logging.h"
#include "chrome/browser/browser_accessibility_manager.h"
#include "chrome/common/accessibility.h"

BrowserAccessibility::BrowserAccessibility()
    : iaccessible_id_(-1),
      instance_active_(true) {
}

HRESULT BrowserAccessibility::accDoDefaultAction(VARIANT var_id) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    // TODO(klink): Once we have MSAA events, change these fails for having
    // BrowserAccessibilityManager firing the right event.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_ACCDODEFAULTACTION, var_id,
                                NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code)
    return S_FALSE;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::accHitTest(LONG x_left, LONG y_top,
                                              VARIANT* child) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!child)
    return E_INVALIDARG;

  if (!parent_hwnd()) {
    // Parent HWND needed for coordinate conversion.
    return E_FAIL;
  }

  // Convert coordinates to test from screen into client window coordinates, to
  // maintain sandbox functionality on renderer side.
  POINT p = {x_left, y_top};
  ::ScreenToClient(parent_hwnd(), &p);

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_ACCHITTEST, EmptyVariant(),
                                p.x, p.y)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // The point is outside of the object's boundaries.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (response().output_long1 == -1) {
    if (CreateInstance(IID_IAccessible, response().iaccessible_id,
                       reinterpret_cast<void**>(&child->pdispVal)) == S_OK) {
      child->vt = VT_DISPATCH;
      // Increment the reference count for the retrieved interface.
      child->pdispVal->AddRef();
    } else {
      return E_NOINTERFACE;
    }
  } else {
    child->vt = VT_I4;
    child->lVal = response().output_long1;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::accLocation(LONG* x_left, LONG* y_top,
                                               LONG* width, LONG* height,
                                               VARIANT var_id) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !x_left || !y_top || !width || !height ||
      !parent_hwnd()) {
    return E_INVALIDARG;
  }

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_ACCLOCATION, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  POINT top_left = {0, 0};

  // Find the top left corner of the containing window in screen coords, and
  // adjust the output position by this amount.
  ::ClientToScreen(parent_hwnd(), &top_left);

  *x_left = response().output_long1 + top_left.x;
  *y_top  = response().output_long2 + top_left.y;

  *width  = response().output_long3;
  *height = response().output_long4;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::accNavigate(LONG nav_dir, VARIANT start,
                                               VARIANT* end) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (start.vt != VT_I4 || !end)
    return E_INVALIDARG;

  if ((nav_dir == NAVDIR_LASTCHILD || nav_dir == NAVDIR_FIRSTCHILD) &&
      start.lVal != CHILDID_SELF) {
    // MSAA states that navigating to first/last child can only be from self.
    return E_INVALIDARG;
  }

  if (nav_dir == NAVDIR_DOWN || nav_dir == NAVDIR_UP ||
      nav_dir == NAVDIR_LEFT || nav_dir == NAVDIR_RIGHT) {
    // Directions not implemented, matching Mozilla and IE.
    return E_INVALIDARG;
  }

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_ACCNAVIGATE, start, nav_dir,
                                NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No screen element was found in the specified direction.
    end->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (response().output_long1 == -1) {
    if (CreateInstance(IID_IAccessible, response().iaccessible_id,
                       reinterpret_cast<void**>(&end->pdispVal)) == S_OK) {
      end->vt = VT_DISPATCH;
      // Increment the reference count for the retrieved interface.
      end->pdispVal->AddRef();
    } else {
      return E_NOINTERFACE;
    }
  } else {
    end->vt = VT_I4;
    end->lVal = response().output_long1;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accChild(VARIANT var_child,
                                                IDispatch** disp_child) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_child.vt != VT_I4 || !disp_child)
    return E_INVALIDARG;

  // If var_child is the parent, remain with the same IDispatch.
  if (var_child.lVal == CHILDID_SELF && iaccessible_id_ != 0)
    return S_OK;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCCHILD, var_child, NULL,
                                NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // When at a leaf, children are handled by the parent object.
    *disp_child = NULL;
    return S_FALSE;
  }

  // Retrieve the IUnknown interface for the parent view, and assign the
  // IDispatch returned.
  if (CreateInstance(IID_IAccessible, response().iaccessible_id,
                     reinterpret_cast<void**>(disp_child)) == S_OK) {
    // Increment the reference count for the retrieved interface.
    (*disp_child)->AddRef();
    return S_OK;
  } else {
    return E_NOINTERFACE;
  }
}

STDMETHODIMP BrowserAccessibility::get_accChildCount(LONG* child_count) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!child_count)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCCHILDCOUNT,
                                EmptyVariant(), NULL, NULL)) {
    return E_FAIL;
  }

  *child_count = response().output_long1;
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accDefaultAction(VARIANT var_id,
                                                        BSTR* def_action) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !def_action)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCDEFAULTACTION, var_id,
                                NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No string found.
    return S_FALSE;
  }

  *def_action = CComBSTR(response().output_string.c_str()).Detach();

  DCHECK(*def_action);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accDescription(VARIANT var_id,
                                                      BSTR* desc) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !desc)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCDESCRIPTION, var_id,
                                NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No string found.
    return S_FALSE;
  }

  *desc = CComBSTR(response().output_string.c_str()).Detach();

  DCHECK(*desc);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accFocus(VARIANT* focus_child) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!focus_child)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCFOCUS, EmptyVariant(),
                                NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // The window that contains this object is not the active window.
    focus_child->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (response().output_long1 == -1) {
    if (CreateInstance(IID_IAccessible, response().iaccessible_id,
           reinterpret_cast<void**>(&focus_child->pdispVal)) == S_OK) {
      focus_child->vt = VT_DISPATCH;
      // Increment the reference count for the retrieved interface.
      focus_child->pdispVal->AddRef();
    } else {
      return E_NOINTERFACE;
    }
  } else {
    focus_child->vt = VT_I4;
    focus_child->lVal = response().output_long1;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accHelp(VARIANT var_id, BSTR* help) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !help)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCHELP, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No string found.
    return S_FALSE;
  }

  *help = CComBSTR(response().output_string.c_str()).Detach();

  DCHECK(*help);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accKeyboardShortcut(VARIANT var_id,
                                                           BSTR* acc_key) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !acc_key)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCKEYBOARDSHORTCUT,
                                var_id, NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No string found.
    return S_FALSE;
  }

  *acc_key = CComBSTR(response().output_string.c_str()).Detach();

  DCHECK(*acc_key);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accName(VARIANT var_id, BSTR* name) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !name)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCNAME, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No string found.
    return S_FALSE;
  }

  *name = CComBSTR(response().output_string.c_str()).Detach();

  DCHECK(*name);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accParent(IDispatch** disp_parent) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (!disp_parent || !parent_hwnd())
    return E_INVALIDARG;

  // Root node's parent is the containing HWND's IAccessible.
  if (iaccessible_id() == 0) {
    // For an object that has no parent (e.g. root), point the accessible parent
    // to the default implementation.
    HRESULT hr =
        ::CreateStdAccessibleObject(parent_hwnd(), OBJID_WINDOW,
                                    IID_IAccessible,
                                    reinterpret_cast<void**>(disp_parent));

    if (!SUCCEEDED(hr)) {
      *disp_parent = NULL;
      return S_FALSE;
    }
    return S_OK;
  }

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCPARENT, EmptyVariant(),
                                NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No parent exists for this object.
    return S_FALSE;
  }

  // Retrieve the IUnknown interface for the parent view, and assign the
  // IDispatch returned.
  if (CreateInstance(IID_IAccessible, response().iaccessible_id,
                     reinterpret_cast<void**>(disp_parent)) == S_OK) {
    // Increment the reference count for the retrieved interface.
    (*disp_parent)->AddRef();
    return S_OK;
  } else {
    return E_NOINTERFACE;
  }
}

STDMETHODIMP BrowserAccessibility::get_accRole(VARIANT var_id, VARIANT* role) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !role)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCROLE, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  role->vt = VT_I4;
  role->lVal = response().output_long1;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accState(VARIANT var_id,
                                                VARIANT* state) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !state)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCSTATE, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  state->vt = VT_I4;
  state->lVal = response().output_long1;

  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accValue(VARIANT var_id, BSTR* value) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !value)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(IACCESSIBLE_FUNC_GET_ACCVALUE, var_id, NULL,
                                NULL)) {
        return E_FAIL;
  }

  if (!response().return_code) {
    // No string found.
    return S_FALSE;
  }

  *value = CComBSTR(response().output_string.c_str()).Detach();

  DCHECK(*value);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::accSelect(LONG flags_select,
                                             VARIANT var_id) {
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP BrowserAccessibility::get_accHelpTopic(BSTR* help_file,
                                                    VARIANT var_id,
                                                    LONG* topic_id) {
  if (help_file) {
    *help_file = NULL;
  }
  if (topic_id) {
    *topic_id = static_cast<LONG>(-1);
  }
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP BrowserAccessibility::get_accSelection(VARIANT* selected) {
  if (selected)
    selected->vt = VT_EMPTY;

  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP BrowserAccessibility::put_accName(VARIANT var_id, BSTR put_name) {
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP BrowserAccessibility::put_accValue(VARIANT var_id, BSTR put_val) {
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP BrowserAccessibility::CreateInstance(REFIID iid,
                                                  int iaccessible_id,
                                                  void** interface_ptr) {
  return BrowserAccessibilityManager::GetInstance()->
      CreateAccessibilityInstance(iid, iaccessible_id, instance_id(),
                                  interface_ptr);
}

bool BrowserAccessibility::RequestAccessibilityInfo(int iaccessible_func_id,
                                                    VARIANT var_id, LONG input1,
                                                    LONG input2) {
  return BrowserAccessibilityManager::GetInstance()->RequestAccessibilityInfo(
      iaccessible_id(), instance_id(), iaccessible_func_id, var_id, input1,
      input2);
}

const AccessibilityOutParams& BrowserAccessibility::response() {
  return BrowserAccessibilityManager::GetInstance()->response();
}

HWND BrowserAccessibility::parent_hwnd() {
  return BrowserAccessibilityManager::GetInstance()->parent_hwnd(instance_id());
}

