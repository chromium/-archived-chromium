// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/accessibility/view_accessibility.h"

#include "chrome/views/accessibility/view_accessibility_wrapper.h"
#include "chrome/views/widget/widget.h"

HRESULT ViewAccessibility::Initialize(views::View* view) {
  if (!view) {
    return E_INVALIDARG;
  }

  view_ = view;
  return S_OK;
}

// TODO(klink): Handle case where child View is not contained by parent.
STDMETHODIMP ViewAccessibility::accHitTest(LONG x_left, LONG y_top,
                                           VARIANT* child) {
  if (!child) {
    return E_INVALIDARG;
  }

  gfx::Point pt(x_left, y_top);
  views::View::ConvertPointToView(NULL, view_, &pt);

  if (!view_->HitTest(pt)) {
    // If containing parent is not hit, return with failure.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  int child_count = view_->GetChildViewCount();
  bool child_hit = false;
  views::View* child_view = NULL;
  for (int child_id = 0; child_id < child_count; ++child_id) {
    // Search for hit within any of the children.
    child_view = view_->GetChildViewAt(child_id);
    views::View::ConvertPointToView(view_, child_view, &pt);
    if (child_view->HitTest(pt)) {
      // Store child_id (adjusted with +1 to convert to MSAA indexing).
      child->lVal = child_id + 1;
      child_hit = true;
      break;
    }
    // Convert point back to parent view to test next child.
    views::View::ConvertPointToView(child_view, view_, &pt);
  }

  child->vt = VT_I4;

  if (!child_hit) {
    // No child hit, return parent id.
    child->lVal = CHILDID_SELF;
  } else {
    if (child_view == NULL) {
      return E_FAIL;
    } else if (child_view->GetChildViewCount() != 0) {
      // Retrieve IDispatch for child, if it is not a leaf.
      child->vt = VT_DISPATCH;
      if ((GetViewAccessibilityWrapper(child_view))->
          GetInstance(IID_IAccessible,
                      reinterpret_cast<void**>(&child->pdispVal)) == S_OK) {
        // Increment the reference count for the retrieved interface.
        child->pdispVal->AddRef();
        return S_OK;
      } else {
        return E_NOINTERFACE;
      }
    }
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::accLocation(LONG* x_left, LONG* y_top,
                                            LONG* width, LONG* height,
                                            VARIANT var_id) {
  if (var_id.vt != VT_I4 || !x_left || !y_top || !width || !height) {
    return E_INVALIDARG;
  }

  gfx::Rect view_bounds;
  // Retrieving the parent View to be used for converting from view-to-screen
  // coordinates.
  views::View* parent = view_->GetParent();

  if (parent == NULL) {
    // If no parent, remain within the same View.
    parent = view_;
  }

  if (var_id.lVal == CHILDID_SELF) {
    // Retrieve active View's bounds.
    view_bounds = view_->bounds();
  } else {
    // Check to see if child is out-of-bounds.
    if (!IsValidChild((var_id.lVal - 1), view_)) {
      return E_INVALIDARG;
    }
    // Retrieve child bounds.
    view_bounds = view_->GetChildViewAt(var_id.lVal - 1)->bounds();
    // Parent View is current View.
    parent = view_;
  }

  if (!view_bounds.IsEmpty()) {
    *width  = view_bounds.width();
    *height = view_bounds.height();

    gfx::Point topleft(view_bounds.origin());
    views::View::ConvertPointToScreen(parent, &topleft);
    *x_left = topleft.x();
    *y_top  = topleft.y();
  } else {
    return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP ViewAccessibility::accNavigate(LONG nav_dir, VARIANT start,
                                            VARIANT* end) {
  if (start.vt != VT_I4 || !end) {
    return E_INVALIDARG;
  }

  switch (nav_dir) {
    case NAVDIR_FIRSTCHILD:
    case NAVDIR_LASTCHILD: {
      if (start.lVal != CHILDID_SELF) {
        // Start of navigation must be on the View itself.
        return E_INVALIDARG;
      } else if (view_->GetChildViewCount() == 0) {
        // No children found.
        return S_FALSE;
      }

      // Set child_id based on first or last child.
      int child_id = 0;
      if (nav_dir == NAVDIR_LASTCHILD) {
        child_id = view_->GetChildViewCount() - 1;
      }

      views::View* child = view_->GetChildViewAt(child_id);

      if (child->GetChildViewCount() != 0) {
        end->vt = VT_DISPATCH;
        if ((GetViewAccessibilityWrapper(child))->
            GetInstance(IID_IAccessible,
                        reinterpret_cast<void**>(&end->pdispVal)) == S_OK) {
          // Increment the reference count for the retrieved interface.
          end->pdispVal->AddRef();
          return S_OK;
        } else {
          return E_NOINTERFACE;
        }
      } else {
        end->vt = VT_I4;
        // Set return child lVal, adjusted for MSAA indexing. (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        end->lVal = child_id + 1;
      }
      break;
    }
    case NAVDIR_LEFT:
    case NAVDIR_UP:
    case NAVDIR_PREVIOUS:
    case NAVDIR_RIGHT:
    case NAVDIR_DOWN:
    case NAVDIR_NEXT: {
      // Retrieve parent to access view index and perform bounds checking.
      views::View* parent = view_->GetParent();
      if (!parent) {
        return E_FAIL;
      }

      if (start.lVal == CHILDID_SELF) {
        int view_index = parent->GetChildIndex(view_);
        // Check navigation bounds, adjusting for View child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, view_index, -1,
                        parent->GetChildViewCount())) {
          // Navigation attempted to go out-of-bounds.
          end->vt = VT_EMPTY;
          return S_FALSE;
        } else {
          if (IsNavDirNext(nav_dir)) {
            view_index += 1;
          } else {
            view_index -=1;
          }
        }

        views::View* child = parent->GetChildViewAt(view_index);
        if (child->GetChildViewCount() != 0) {
          end->vt = VT_DISPATCH;
          // Retrieve IDispatch for non-leaf child.
          if ((GetViewAccessibilityWrapper(child))->
              GetInstance(IID_IAccessible,
                          reinterpret_cast<void**>(&end->pdispVal)) == S_OK) {
            // Increment the reference count for the retrieved interface.
            end->pdispVal->AddRef();
            return S_OK;
          } else {
            return E_NOINTERFACE;
          }
        } else {
          end->vt = VT_I4;
          // Modifying view_index to give lVal correct MSAA-based value. (MSAA
          // child indexing starts with 1, whereas View indexing starts with 0).
          end->lVal = view_index + 1;
        }
      } else {
        // Check navigation bounds, adjusting for MSAA child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, start.lVal, 0,
                        parent->GetChildViewCount() + 1)) {
            // Navigation attempted to go out-of-bounds.
            end->vt = VT_EMPTY;
            return S_FALSE;
          } else {
            if (IsNavDirNext(nav_dir)) {
              start.lVal += 1;
            } else {
              start.lVal -= 1;
            }
        }

        HRESULT result = this->get_accChild(start, &end->pdispVal);
        if (result == S_FALSE) {
          // Child is a leaf.
          end->vt = VT_I4;
          end->lVal = start.lVal;
        } else if (result == E_INVALIDARG) {
          return E_INVALIDARG;
        } else {
          // Child is not a leaf.
          end->vt = VT_DISPATCH;
        }
      }
      break;
    }
    default:
      return E_INVALIDARG;
  }
  // Navigation performed correctly. Global return for this function, if no
  // error triggered an escape earlier.
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accChild(VARIANT var_child,
                                             IDispatch** disp_child) {
  if (var_child.vt != VT_I4 || !disp_child) {
    return E_INVALIDARG;
  }

  // If var_child is the parent, remain with the same IDispatch.
  if (var_child.lVal == CHILDID_SELF) {
    return S_OK;
  }

  views::View* child_view = NULL;
  bool get_iaccessible = false;

  // Check to see if child is out-of-bounds.
  if (IsValidChild((var_child.lVal - 1), view_)) {
    child_view = view_->GetChildViewAt(var_child.lVal - 1);
  } else {
    // Child is located elsewhere in the hierarchy, get ID and adjust for MSAA.
    child_view = view_->GetViewByID(static_cast<int>(var_child.lVal));
    get_iaccessible = true;
  }

  if (!child_view) {
    // No child found.
    *disp_child = NULL;
    return E_FAIL;
  }

  if (get_iaccessible || child_view->GetChildViewCount() != 0) {
    // Retrieve the IUnknown interface for the requested child view, and
    // assign the IDispatch returned.
    if ((GetViewAccessibilityWrapper(child_view))->
        GetInstance(IID_IAccessible,
                    reinterpret_cast<void**>(disp_child)) == S_OK) {
      // Increment the reference count for the retrieved interface.
      (*disp_child)->AddRef();
      return S_OK;
    } else {
      // No interface, return failure.
      return E_NOINTERFACE;
    }
  } else {
    // When at a leaf, children are handled by the parent object.
    *disp_child = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP ViewAccessibility::get_accChildCount(LONG* child_count) {
  if (!child_count || !view_) {
    return E_INVALIDARG;
  }

  *child_count = view_->GetChildViewCount();
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accDefaultAction(VARIANT var_id,
                                                     BSTR* def_action) {
  if (var_id.vt != VT_I4 || !def_action) {
    return E_INVALIDARG;
  }

  std::wstring temp_action;

  if (var_id.lVal == CHILDID_SELF) {
    view_->GetAccessibleDefaultAction(&temp_action);
  } else {
    if (!IsValidChild((var_id.lVal - 1), view_)) {
      return E_INVALIDARG;
    }
    view_->GetChildViewAt(var_id.lVal - 1)->
        GetAccessibleDefaultAction(&temp_action);
  }
  if (!temp_action.empty()) {
    *def_action = CComBSTR(temp_action.c_str()).Detach();
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accDescription(VARIANT var_id, BSTR* desc) {
  if (var_id.vt != VT_I4 || !desc) {
    return E_INVALIDARG;
  }

  std::wstring temp_desc;

  if (var_id.lVal == CHILDID_SELF) {
    view_->GetTooltipText(0, 0, &temp_desc);
  } else {
    if (!IsValidChild((var_id.lVal - 1), view_)) {
      return E_INVALIDARG;
    }
    view_->GetChildViewAt(var_id.lVal - 1)->GetTooltipText(0, 0, &temp_desc);
  }
  if (!temp_desc.empty()) {
    *desc = CComBSTR(temp_desc.c_str()).Detach();
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accFocus(VARIANT* focus_child) {
  if (!focus_child) {
    return E_INVALIDARG;
  }

  if (view_->GetChildViewCount() == 0 && view_->HasFocus()) {
    // Parent view has focus.
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else {
    bool has_focus = false;
    int child_count = view_->GetChildViewCount();
    // Search for child view with focus.
    for (int child_id = 0; child_id < child_count; ++child_id) {
      if (view_->GetChildViewAt(child_id)->HasFocus()) {
        focus_child->vt = VT_I4;
        focus_child->lVal = child_id + 1;

        // If child view is no leaf, retrieve IDispatch.
        if (view_->GetChildViewAt(child_id)->GetChildViewCount() != 0) {
          focus_child->vt = VT_DISPATCH;
          this->get_accChild(*focus_child, &focus_child->pdispVal);
        }
        has_focus = true;
        break;
      }
    }
    // No current focus on any of the children.
    if (!has_focus) {
      focus_child->vt = VT_EMPTY;
      return S_FALSE;
    }
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accKeyboardShortcut(VARIANT var_id,
                                                        BSTR* acc_key) {
  if (var_id.vt != VT_I4 || !acc_key) {
    return E_INVALIDARG;
  }

  std::wstring temp_key;

  if (var_id.lVal == CHILDID_SELF) {
    view_->GetAccessibleKeyboardShortcut(&temp_key);
  } else {
    if (!IsValidChild((var_id.lVal - 1), view_)) {
      return E_INVALIDARG;
    }
    view_->GetChildViewAt(var_id.lVal - 1)->
        GetAccessibleKeyboardShortcut(&temp_key);
  }
  if (!temp_key.empty()) {
    *acc_key = CComBSTR(temp_key.c_str()).Detach();
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accName(VARIANT var_id, BSTR* name) {
  if (var_id.vt != VT_I4 || !name) {
    return E_INVALIDARG;
  }

  std::wstring temp_name;

  if (var_id.lVal == CHILDID_SELF) {
    // Retrieve the parent view's name.
    view_->GetAccessibleName(&temp_name);
  } else {
    if (!IsValidChild((var_id.lVal - 1), view_)) {
      return E_INVALIDARG;
    }
    // Retrieve the child view's name.
    view_->GetChildViewAt(var_id.lVal - 1)->GetAccessibleName(&temp_name);
  }
  if (!temp_name.empty()) {
    // Return name retrieved.
    *name = CComBSTR(temp_name.c_str()).Detach();
  } else {
    // If view has no name, return S_FALSE.
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accParent(IDispatch** disp_parent) {
  if (!disp_parent) {
    return E_INVALIDARG;
  }

  views::View* parent_view = view_->GetParent();

  if (!parent_view) {
    // This function can get called during teardown of WidetWin so we
    // should bail out if we fail to get the HWND.
    if (!view_->GetWidget() || !view_->GetWidget()->GetNativeView()) {
      *disp_parent = NULL;
      return S_FALSE;
    }

    // For a View that has no parent (e.g. root), point the accessible parent to
    // the default implementation, to interface with Windows' hierarchy and to
    // support calls from e.g. WindowFromAccessibleObject.
    HRESULT hr =
        ::AccessibleObjectFromWindow(view_->GetWidget()->GetNativeView(),
                                     OBJID_WINDOW, IID_IAccessible,
                                     reinterpret_cast<void**>(disp_parent));

    if (!SUCCEEDED(hr)) {
      *disp_parent = NULL;
      return S_FALSE;
    }

    return S_OK;
  }

  // Retrieve the IUnknown interface for the parent view, and assign the
  // IDispatch returned.
  if ((GetViewAccessibilityWrapper(parent_view))->
      GetInstance(IID_IAccessible,
                  reinterpret_cast<void**>(disp_parent)) == S_OK) {
    // Increment the reference count for the retrieved interface.
    (*disp_parent)->AddRef();
    return S_OK;
  } else {
    return E_NOINTERFACE;
  }
}

STDMETHODIMP ViewAccessibility::get_accRole(VARIANT var_id, VARIANT* role) {
  if (var_id.vt != VT_I4 || !role) {
    return E_INVALIDARG;
  }

  if (var_id.lVal == CHILDID_SELF) {
    // Retrieve parent role.
    if (!view_->GetAccessibleRole(role)) {
      return E_FAIL;
    }
  } else {
    if (!IsValidChild((var_id.lVal - 1), view_)) {
      return E_INVALIDARG;
    }
    // Retrieve child role.
    if (!view_->GetChildViewAt(var_id.lVal - 1)->GetAccessibleRole(role)) {
      return E_FAIL;
    }
  }

  // Make sure that role is not empty, and has the proper type.
  if (role->vt == VT_EMPTY)
    return E_FAIL;

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accState(VARIANT var_id, VARIANT* state) {
  if (var_id.vt != VT_I4 || !state) {
    return E_INVALIDARG;
  }

  state->vt = VT_I4;

  if (var_id.lVal == CHILDID_SELF) {
    // Retrieve all currently applicable states of the parent.
    this->SetState(state, view_);
  } else {
    if (!IsValidChild((var_id.lVal - 1), view_)) {
      return E_INVALIDARG;
    }
    // Retrieve all currently applicable states of the child.
    this->SetState(state, view_->GetChildViewAt(var_id.lVal - 1));
  }

  // Make sure that state is not empty, and has the proper type.
  if (state->vt == VT_EMPTY)
    return E_FAIL;

  return S_OK;
}

// Helper functions.

bool ViewAccessibility::IsValidChild(int child_id, views::View* view) const {
  if (((child_id) < 0) ||
      ((child_id) >= view->GetChildViewCount())) {
    return false;
  }
  return true;
}

bool ViewAccessibility::IsNavDirNext(int nav_dir) const {
  if (nav_dir == NAVDIR_RIGHT || nav_dir == NAVDIR_DOWN ||
      nav_dir == NAVDIR_NEXT) {
      return true;
  }
  return false;
}

bool ViewAccessibility::IsValidNav(int nav_dir, int start_id, int lower_bound,
                                   int upper_bound) const {
  if (IsNavDirNext(nav_dir)) {
    if ((start_id + 1) > upper_bound) {
      return false;
    }
  } else {
    if ((start_id - 1) <= lower_bound) {
      return false;
    }
  }
  return true;
}

void ViewAccessibility::SetState(VARIANT* state, views::View* view) {
  // Default state; all views can have accessibility focus.
  state->lVal |= STATE_SYSTEM_FOCUSABLE;

  if (!view)
    return;

  if (!view->IsEnabled()) {
      state->lVal |= STATE_SYSTEM_UNAVAILABLE;
  }
  if (!view->IsVisible()) {
    state->lVal |= STATE_SYSTEM_INVISIBLE;
  }
  if (view->IsHotTracked()) {
    state->lVal |= STATE_SYSTEM_HOTTRACKED;
  }
  if (view->IsPushed()) {
    state->lVal |= STATE_SYSTEM_PRESSED;
  }
  // Check both for actual View focus, as well as accessibility focus.
  views::View* parent = view->GetParent();

  if (view->HasFocus() ||
      (parent && parent->GetAccFocusedChildView() == view)) {
    state->lVal |= STATE_SYSTEM_FOCUSED;
  }

  // Add on any view-specific states.
  view->GetAccessibleState(state);
}

// IAccessible functions not supported.

HRESULT ViewAccessibility::accDoDefaultAction(VARIANT var_id) {
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::get_accValue(VARIANT var_id, BSTR* value) {
  if (value)
    *value = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::get_accSelection(VARIANT* selected) {
  if (selected)
    selected->vt = VT_EMPTY;
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::accSelect(LONG flagsSelect, VARIANT var_id) {
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::get_accHelp(VARIANT var_id, BSTR* help) {
  if (help)
    *help = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::get_accHelpTopic(BSTR* help_file,
                                                 VARIANT var_id,
                                                 LONG* topic_id) {
  if (help_file) {
    *help_file = NULL;
  }
  if (topic_id) {
    *topic_id = static_cast<LONG>(-1);
  }
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::put_accName(VARIANT var_id, BSTR put_name) {
  // Deprecated.
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::put_accValue(VARIANT var_id, BSTR put_val) {
  // Deprecated.
  return E_NOTIMPL;
}
