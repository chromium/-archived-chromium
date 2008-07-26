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

#include "chrome/views/accessibility/accessible_wrapper.h"

#include "base/logging.h"
#include "chrome/views/accessibility/view_accessibility.h"

////////////////////////////////////////////////////////////////////////////////
//
// AccessibleWrapper - constructors, destructors
//
////////////////////////////////////////////////////////////////////////////////

AccessibleWrapper::AccessibleWrapper(ChromeViews::View* view) :
    accessibility_info_(NULL),
    view_(view) {
}

STDMETHODIMP AccessibleWrapper::CreateDefaultInstance(REFIID iid) {
  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid) {
    // If there is no instance of ViewAccessibility created, create it
    // now. Otherwise reuse previous instance.
    if (!accessibility_info_) {
      CComObject<ViewAccessibility>* instance = NULL;

      HRESULT hr = CComObject<ViewAccessibility>::CreateInstance(&instance);
      DCHECK(SUCCEEDED(hr));

      if (!instance)
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

STDMETHODIMP AccessibleWrapper::GetInstance(REFIID iid, void** interface_ptr) {
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

STDMETHODIMP AccessibleWrapper::SetInstance(IAccessible* interface_ptr) {
  if (!interface_ptr)
    return E_NOINTERFACE;

  accessibility_info_.Attach(interface_ptr);

  // Paranoia check, to make sure we do have a valid IAccessible pointer stored.
  if (!accessibility_info_)
    return E_FAIL;

  return S_OK;
}
