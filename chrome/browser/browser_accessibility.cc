// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility.h"

#include "base/logging.h"
#include "chrome/browser/browser_accessibility_manager.h"

using webkit_glue::WebAccessibility;

HRESULT BrowserAccessibility::Initialize(int iaccessible_id, int routing_id,
                                         int process_id, HWND parent_hwnd) {
  // Check input parameters. Root id is 1000, to avoid conflicts with the ids
  // used by MSAA.
  if (!parent_hwnd || iaccessible_id < 1000)
    return E_INVALIDARG;

  iaccessible_id_ = iaccessible_id;
  routing_id_ = routing_id;
  process_id_ = process_id;
  parent_hwnd_ = parent_hwnd;

  // Mark instance as active.
  instance_active_ = true;
  return S_OK;
}

HRESULT BrowserAccessibility::accDoDefaultAction(VARIANT var_id) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    // TODO(klink): Once we have MSAA events, change these fails to having
    // BrowserAccessibilityManager firing the right event.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_DODEFAULTACTION,
                                var_id, NULL, NULL)) {
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

  if (!parent_hwnd_) {
    // Parent HWND needed for coordinate conversion.
    return E_FAIL;
  }

  // Convert coordinates to test from screen into client window coordinates, to
  // maintain sandbox functionality on renderer side.
  POINT p = {x_left, y_top};
  ::ScreenToClient(parent_hwnd_, &p);

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_HITTEST,
                                EmptyVariant(), p.x, p.y)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // The point is outside of the object's boundaries.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (response().output_long1 == -1) {
    if (CreateInstance(IID_IAccessible, response().object_id,
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
      !parent_hwnd_) {
    return E_INVALIDARG;
  }

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_LOCATION, var_id,
                                NULL, NULL)) {
    return E_FAIL;
  }

  POINT top_left = {0, 0};

  // Find the top left corner of the containing window in screen coords, and
  // adjust the output position by this amount.
  ::ClientToScreen(parent_hwnd_, &top_left);

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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_NAVIGATE, start,
                                nav_dir, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No screen element was found in the specified direction.
    end->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (response().output_long1 == -1) {
    if (CreateInstance(IID_IAccessible, response().object_id,
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
  if (var_child.lVal == CHILDID_SELF && iaccessible_id_ != 1000)
    return S_OK;

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_GETCHILD, var_child,
                                NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // When at a leaf, children are handled by the parent object.
    *disp_child = NULL;
    return S_FALSE;
  }

  // Retrieve the IUnknown interface for the parent view, and assign the
  // IDispatch returned.
  if (CreateInstance(IID_IAccessible, response().object_id,
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_CHILDCOUNT,
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_DEFAULTACTION,
                                var_id, NULL, NULL)) {
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_DESCRIPTION, var_id,
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_GETFOCUSEDCHILD,
                                EmptyVariant(), NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // The window that contains this object is not the active window.
    focus_child->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (response().output_long1 == -1) {
    if (CreateInstance(IID_IAccessible, response().object_id,
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_HELPTEXT, var_id,
                                NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code || response().output_string.empty()) {
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_KEYBOARDSHORTCUT,
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_NAME, var_id, NULL,
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

  if (!disp_parent || !parent_hwnd_)
    return E_INVALIDARG;

  // Root node's parent is the containing HWND's IAccessible.
  if (iaccessible_id_ == 1000) {
    // For an object that has no parent (e.g. root), point the accessible parent
    // to the default implementation.
    HRESULT hr =
        ::CreateStdAccessibleObject(parent_hwnd_, OBJID_WINDOW,
                                    IID_IAccessible,
                                    reinterpret_cast<void**>(disp_parent));

    if (!SUCCEEDED(hr)) {
      *disp_parent = NULL;
      return S_FALSE;
    }
    return S_OK;
  }

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_GETPARENT,
                                EmptyVariant(), NULL, NULL)) {
    return E_FAIL;
  }

  if (!response().return_code) {
    // No parent exists for this object.
    return S_FALSE;
  }

  // Retrieve the IUnknown interface for the parent view, and assign the
  // IDispatch returned.
  if (CreateInstance(IID_IAccessible, response().object_id,
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_ROLE, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  role->vt = VT_I4;
  role->lVal = MSAARole(response().output_long1);
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

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_STATE, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  state->vt = VT_I4;
  state->lVal = MSAAState(response().output_long1);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accValue(VARIANT var_id, BSTR* value) {
  if (!instance_active()) {
    // Instance no longer active, fail gracefully.
    return E_FAIL;
  }

  if (var_id.vt != VT_I4 || !value)
    return E_INVALIDARG;

  if (!RequestAccessibilityInfo(WebAccessibility::FUNCTION_VALUE, var_id, NULL,
                                NULL)) {
    return E_FAIL;
  }

  if (!response().return_code || response().output_string.empty()) {
    // No string found.
    return S_FALSE;
  }

  *value = CComBSTR(response().output_string.c_str()).Detach();

  DCHECK(*value);
  return S_OK;
}

STDMETHODIMP BrowserAccessibility::get_accHelpTopic(BSTR* help_file,
                                                    VARIANT var_id,
                                                    LONG* topic_id) {
  if (help_file)
    *help_file = NULL;

  if (topic_id)
    *topic_id = static_cast<LONG>(-1);

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::get_accSelection(VARIANT* selected) {
  if (selected)
    selected->vt = VT_EMPTY;

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibility::CreateInstance(REFIID iid,
                                                  int iaccessible_id,
                                                  void** interface_ptr) {
  return BrowserAccessibilityManager::GetInstance()->
      CreateAccessibilityInstance(iid, iaccessible_id, routing_id_,
                                  process_id_, parent_hwnd_, interface_ptr);
}

bool BrowserAccessibility::RequestAccessibilityInfo(int iaccessible_func_id,
                                                    VARIANT var_id, LONG input1,
                                                    LONG input2) {
  // Create and populate IPC message structure, for retrieval of accessibility
  // information from the renderer.
  WebAccessibility::InParams in_params;
  in_params.object_id = iaccessible_id_;
  in_params.function_id = iaccessible_func_id;
  in_params.child_id = var_id.lVal;
  in_params.input_long1 = input1;
  in_params.input_long2 = input2;

  return BrowserAccessibilityManager::GetInstance()->
      RequestAccessibilityInfo(&in_params, routing_id_, process_id_);
}

const WebAccessibility::OutParams& BrowserAccessibility::response() {
  return BrowserAccessibilityManager::GetInstance()->response();
}

long BrowserAccessibility::MSAARole(long browser_accessibility_role) {
  switch (browser_accessibility_role) {
    case WebAccessibility::ROLE_CELL:
      return ROLE_SYSTEM_CELL;
    case WebAccessibility::ROLE_CHECKBUTTON:
      return ROLE_SYSTEM_CHECKBUTTON;
    case WebAccessibility::ROLE_COLUMN:
      return ROLE_SYSTEM_COLUMN;
    case WebAccessibility::ROLE_COLUMNHEADER:
      return ROLE_SYSTEM_COLUMNHEADER;
    case WebAccessibility::ROLE_GRAPHIC:
      return ROLE_SYSTEM_GRAPHIC;
    case WebAccessibility::ROLE_GROUPING:
      return ROLE_SYSTEM_GROUPING;
    case WebAccessibility::ROLE_LINK:
      return ROLE_SYSTEM_LINK;
    case WebAccessibility::ROLE_LIST:
    case WebAccessibility::ROLE_LISTBOX:
      return ROLE_SYSTEM_LIST;
    case WebAccessibility::ROLE_MENUITEM:
      return ROLE_SYSTEM_MENUITEM;
    case WebAccessibility::ROLE_MENUPOPUP:
      return ROLE_SYSTEM_MENUPOPUP;
    case WebAccessibility::ROLE_OUTLINE:
      return ROLE_SYSTEM_OUTLINE;
    case WebAccessibility::ROLE_PAGETABLIST:
      return ROLE_SYSTEM_PAGETABLIST;
    case WebAccessibility::ROLE_PROGRESSBAR:
      return ROLE_SYSTEM_PROGRESSBAR;
    case WebAccessibility::ROLE_PUSHBUTTON:
      return ROLE_SYSTEM_PUSHBUTTON;
    case WebAccessibility::ROLE_RADIOBUTTON:
      return ROLE_SYSTEM_RADIOBUTTON;
    case WebAccessibility::ROLE_ROW:
      return ROLE_SYSTEM_ROW;
    case WebAccessibility::ROLE_ROWHEADER:
      return ROLE_SYSTEM_ROWHEADER;
    case WebAccessibility::ROLE_SLIDER:
      return ROLE_SYSTEM_SLIDER;
    case WebAccessibility::ROLE_STATICTEXT:
      return ROLE_SYSTEM_STATICTEXT;
    case WebAccessibility::ROLE_TABLE:
      return ROLE_SYSTEM_TABLE;
    case WebAccessibility::ROLE_TEXT:
      return ROLE_SYSTEM_TEXT;
    case WebAccessibility::ROLE_CLIENT:
    default:
      // This is the default role for MSAA.
      return ROLE_SYSTEM_CLIENT;
  }
}

long BrowserAccessibility::MSAAState(long browser_accessibility_state) {
  long state = 0;

  if ((browser_accessibility_state >> WebAccessibility::STATE_CHECKED) & 1)
    state |= STATE_SYSTEM_CHECKED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_FOCUSABLE) & 1)
    state |= STATE_SYSTEM_FOCUSABLE;

  if ((browser_accessibility_state >> WebAccessibility::STATE_FOCUSED) & 1)
    state |= STATE_SYSTEM_FOCUSED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_HOTTRACKED) & 1)
    state |= STATE_SYSTEM_HOTTRACKED;

  if ((browser_accessibility_state >>
       WebAccessibility::STATE_INDETERMINATE) & 1) {
    state |= STATE_SYSTEM_INDETERMINATE;
  }

  if ((browser_accessibility_state >> WebAccessibility::STATE_LINKED) & 1)
    state |= STATE_SYSTEM_LINKED;

  if ((browser_accessibility_state >>
       WebAccessibility::STATE_MULTISELECTABLE) & 1) {
    state |= STATE_SYSTEM_MULTISELECTABLE;
  }

  if ((browser_accessibility_state >> WebAccessibility::STATE_OFFSCREEN) & 1)
    state |= STATE_SYSTEM_OFFSCREEN;

  if ((browser_accessibility_state >> WebAccessibility::STATE_PRESSED) & 1)
    state |= STATE_SYSTEM_PRESSED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_PROTECTED) & 1)
    state |= STATE_SYSTEM_PROTECTED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_READONLY) & 1)
    state |= STATE_SYSTEM_READONLY;

  if ((browser_accessibility_state >> WebAccessibility::STATE_TRAVERSED) & 1)
    state |= STATE_SYSTEM_TRAVERSED;

  if ((browser_accessibility_state >> WebAccessibility::STATE_UNAVAILABLE) & 1)
    state |= STATE_SYSTEM_UNAVAILABLE;

  return state;
}
