// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_ACCESSIBILITY_ACCESSIBLE_WRAPPER_H_
#define CHROME_VIEWS_ACCESSIBILITY_ACCESSIBLE_WRAPPER_H_

#include <atlcomcli.h>
#include <oleacc.h>

#include "base/basictypes.h"

namespace views {
class View;
}

////////////////////////////////////////////////////////////////////////////////
//
// AccessibleWrapper
//
// Wrapper class for returning a pointer to the appropriate (platform-specific)
// accessibility interface for a given View. Needed to keep platform-specific
// code out of the View class, when answering calls for child/parent IAccessible
// implementations, for instance.
//
////////////////////////////////////////////////////////////////////////////////
class AccessibleWrapper {
 public:
  explicit AccessibleWrapper(views::View* view);
  ~AccessibleWrapper() {}

  STDMETHODIMP CreateDefaultInstance(REFIID iid);

  // Returns a pointer to a specified interface on an object to which a client
  // currently holds an interface pointer. If pointer exists, it is reused,
  // otherwise a new pointer is created. Used by accessibility implementation to
  // retrieve MSAA implementation for child or parent, when navigating MSAA
  // hierarchy.
  STDMETHODIMP GetInstance(REFIID iid, void** interface_ptr);

  // Sets the accessibility interface implementation of this wrapper to be
  // anything the user specifies.
  STDMETHODIMP SetInstance(IAccessible* interface_ptr);

 private:
  // Instance of accessibility information and handling for a View.
  CComPtr<IAccessible> accessibility_info_;

  // View needed to initialize IAccessible.
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleWrapper);
};

#endif  // CHROME_VIEWS_ACCESSIBILITY_ACCESSIBLE_WRAPPER_H_
