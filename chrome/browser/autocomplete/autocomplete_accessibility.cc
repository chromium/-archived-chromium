// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_accessibility.h"

#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/accessibility/view_accessibility_wrapper.h"
#include "chrome/views/view.h"
#include "grit/generated_resources.h"

HRESULT AutocompleteAccessibility::Initialize(
    const AutocompleteEditViewWin* edit_box) {
  if (edit_box == NULL) {
    return E_INVALIDARG;
  }

  edit_box_ = edit_box;

  // Create a default accessible object for this instance.
  return CreateStdAccessibleObject(edit_box_->m_hWnd, OBJID_CLIENT,
                                   IID_IAccessible, reinterpret_cast<void **>
                                       (&default_accessibility_server_));
}

STDMETHODIMP AutocompleteAccessibility::get_accChildCount(LONG* child_count) {
  if (!child_count) {
    return E_INVALIDARG;
  }

  DCHECK(default_accessibility_server_);
  return default_accessibility_server_->get_accChildCount(child_count);
}

STDMETHODIMP AutocompleteAccessibility::get_accChild(VARIANT var_child,
                                                     IDispatch** disp_child) {
  if (var_child.vt != VT_I4 || !disp_child) {
    return E_INVALIDARG;
  }

  // If var_child is the parent, remain with the same IDispatch
  if (var_child.lVal == CHILDID_SELF)
    return S_OK;

  *disp_child = NULL;
  return S_FALSE;
}

STDMETHODIMP AutocompleteAccessibility::get_accParent(IDispatch** disp_parent) {
  if (!disp_parent) {
    return E_INVALIDARG;
  }

  if (edit_box_->parent_view() == NULL) {
    *disp_parent = NULL;
    return S_FALSE;
  }

  // Retrieve the IUnknown interface for the parent view, and assign the
  // IDispatch returned.
  if (edit_box_->parent_view()->GetViewAccessibilityWrapper()->GetInstance(
      IID_IAccessible, reinterpret_cast<void**>(disp_parent)) == S_OK) {
    // Increment the reference count for the retrieved interface.
    (*disp_parent)->AddRef();
    return S_OK;
  } else {
    return E_NOINTERFACE;
  }
}

STDMETHODIMP AutocompleteAccessibility::accNavigate(LONG nav_dir, VARIANT start,
                                                    VARIANT* end) {
  if (start.vt != VT_I4 || !end) {
    return E_INVALIDARG;
  }

  DCHECK(default_accessibility_server_);
  return default_accessibility_server_->accNavigate(nav_dir, start, end);
}

STDMETHODIMP AutocompleteAccessibility::get_accFocus(VARIANT* focus_child) {
  if (!focus_child) {
    return E_INVALIDARG;
  }

  DCHECK(default_accessibility_server_);
  return default_accessibility_server_->get_accFocus(focus_child);
}

STDMETHODIMP AutocompleteAccessibility::get_accName(VARIANT var_id,
                                                    BSTR* name) {
  if (var_id.vt != VT_I4 || !name) {
    return E_INVALIDARG;
  }

  std::wstring temp_name = l10n_util::GetString(IDS_ACCNAME_LOCATION);

  if (!temp_name.empty()) {
    // Return name retrieved.
    *name = CComBSTR(temp_name.c_str()).Detach();
  } else {
    // If no name is found, return S_FALSE.
    return S_FALSE;
  }
  DCHECK(*name);

  return S_OK;
}

STDMETHODIMP AutocompleteAccessibility::get_accDescription(VARIANT var_id,
                                                           BSTR* desc) {
  if (var_id.vt != VT_I4 || !desc) {
    return E_INVALIDARG;
  }

  return S_FALSE;
}

STDMETHODIMP AutocompleteAccessibility::get_accValue(VARIANT var_id,
                                                     BSTR* value) {
  if (var_id.vt != VT_I4 || !value) {
    return E_INVALIDARG;
  }

  std::wstring temp_value;

  if (var_id.lVal != CHILDID_SELF)
   return E_INVALIDARG;

  // Edit box has no children, only handle self.
  temp_value = edit_box_->GetText();
  if (temp_value.empty())
   return S_FALSE;

   // Return value retrieved.
  *value = CComBSTR(temp_value.c_str()).Detach();

  DCHECK(*value);

  return S_OK;

}

STDMETHODIMP AutocompleteAccessibility::get_accState(VARIANT var_id,
                                                     VARIANT* state) {
  if (var_id.vt != VT_I4 || !state) {
    return E_INVALIDARG;
  }

  DCHECK(default_accessibility_server_);
  HRESULT hr = default_accessibility_server_->get_accState(var_id, state);

  if (hr != S_OK)
    return hr;

  // Adding on state to convey the fact that there is a dropdown.
  state->lVal |= STATE_SYSTEM_HASPOPUP;
  return S_OK;
}

STDMETHODIMP AutocompleteAccessibility::get_accRole(VARIANT var_id,
                                                    VARIANT* role) {
  if (var_id.vt != VT_I4 || !role) {
    return E_INVALIDARG;
  }

  role->vt = VT_I4;

  // Need to override the default role, which is ROLE_SYSTEM_CLIENT.
  if (var_id.lVal == CHILDID_SELF) {
    role->lVal = ROLE_SYSTEM_TEXT;
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP AutocompleteAccessibility::get_accDefaultAction(VARIANT var_id,
                                                             BSTR* def_action) {
  if (var_id.vt != VT_I4 || !def_action) {
    return E_INVALIDARG;
  }

  return S_FALSE;
}

STDMETHODIMP AutocompleteAccessibility::accLocation(LONG* x_left, LONG* y_top,
                                                    LONG* width, LONG* height,
                                                    VARIANT var_id) {
  if (var_id.vt != VT_I4 || !x_left || !y_top || !width || !height) {
    return E_INVALIDARG;
  }

  DCHECK(default_accessibility_server_);
  return default_accessibility_server_->accLocation(x_left, y_top, width,
                                                    height, var_id);
}

STDMETHODIMP AutocompleteAccessibility::accHitTest(LONG x_left, LONG y_top,
                                                   VARIANT* child) {
  if (!child) {
    return E_INVALIDARG;
  }

  DCHECK(default_accessibility_server_);
  return default_accessibility_server_->accHitTest(x_left, y_top, child);
}

STDMETHODIMP AutocompleteAccessibility::get_accKeyboardShortcut(VARIANT var_id,
                                                                BSTR* acc_key) {
  if (var_id.vt != VT_I4 || !acc_key) {
    return E_INVALIDARG;
  }

  return S_FALSE;
}

// IAccessible functions not supported.

HRESULT AutocompleteAccessibility::accDoDefaultAction(VARIANT var_id) {
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP AutocompleteAccessibility::get_accSelection(VARIANT* selected) {
  if (selected)
    selected->vt = VT_EMPTY;
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP AutocompleteAccessibility::accSelect(LONG flagsSelect,
                                                  VARIANT var_id) {
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP AutocompleteAccessibility::get_accHelp(VARIANT var_id,
                                                    BSTR* help) {
  if (help)
    *help = NULL;
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP AutocompleteAccessibility::get_accHelpTopic(BSTR* help_file,
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

STDMETHODIMP AutocompleteAccessibility::put_accName(VARIANT var_id,
                                                    BSTR put_name) {
  // Deprecated.
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP AutocompleteAccessibility::put_accValue(VARIANT var_id,
                                                     BSTR put_val) {
  // Deprecated.
  return DISP_E_MEMBERNOTFOUND;
}
