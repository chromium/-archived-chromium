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

#ifndef CHROME_VIEWS_ACCESSIBILITY_ACCESSIBLE_WRAPPER_H__
#define CHROME_VIEWS_ACCESSIBILITY_ACCESSIBLE_WRAPPER_H__

#include <atlcomcli.h>
#include <oleacc.h>
#include <windows.h>

#include "base/basictypes.h"

namespace ChromeViews {
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
  explicit AccessibleWrapper(ChromeViews::View* view);
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
  ChromeViews::View* view_;

  DISALLOW_EVIL_CONSTRUCTORS(AccessibleWrapper);
};
#endif  // CHROME_VIEWS_ACCESSIBILITY_ACCESSIBLE_WRAPPER_H__
