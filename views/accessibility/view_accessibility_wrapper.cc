// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/accessibility/view_accessibility_wrapper.h"

#include "views/accessibility/view_accessibility.h"

////////////////////////////////////////////////////////////////////////////////
//
// ViewAccessibilityWrapper - constructors, destructors
//
////////////////////////////////////////////////////////////////////////////////

ViewAccessibilityWrapper::ViewAccessibilityWrapper(views::View* view)
    : accessibility_info_(NULL),
      view_(view) {
}

STDMETHODIMP ViewAccessibilityWrapper::CreateDefaultInstance(REFIID iid) {
  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid) {
    // If there is no instance of ViewAccessibility created, create it
    // now. Otherwise reuse previous instance.
    if (!accessibility_info_) {
      CComObject<ViewAccessibility>* instance = NULL;

      HRESULT hr = CComObject<ViewAccessibility>::CreateInstance(&instance);

      if (!SUCCEEDED(hr) || !instance)
        return E_FAIL;

      CComPtr<IAccessible> accessibility_instance(instance);

      if (!SUCCEEDED(instance->Initialize(view_)))
        return E_FAIL;

      // All is well, assign the temp instance to the class smart pointer.
      accessibility_info_.Attach(accessibility_instance.Detach());
    }
    return S_OK;
  }
  // Interface not supported.
  return E_NOINTERFACE;
}

STDMETHODIMP ViewAccessibilityWrapper::GetInstance(REFIID iid,
                                                   void** interface_ptr) {
  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid) {
    // If there is no accessibility instance created, create a default now.
    // Otherwise reuse previous instance.
    if (!accessibility_info_) {
      HRESULT hr = CreateDefaultInstance(iid);

      if (hr != S_OK) {
        // Interface creation failed.
        *interface_ptr = NULL;
        return E_NOINTERFACE;
      }
    }
    *interface_ptr = static_cast<IAccessible*>(accessibility_info_);
    return S_OK;
  }
  // No supported interface found, return error.
  *interface_ptr = NULL;
  return E_NOINTERFACE;
}

STDMETHODIMP ViewAccessibilityWrapper::SetInstance(IAccessible* interface_ptr) {
  if (!interface_ptr)
    return E_NOINTERFACE;

  accessibility_info_.Attach(interface_ptr);

  // Paranoia check, to make sure we do have a valid IAccessible pointer stored.
  if (!accessibility_info_)
    return E_FAIL;

  return S_OK;
}
