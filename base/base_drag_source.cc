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

#include <atlbase.h>

#include "base/base_drag_source.h"

///////////////////////////////////////////////////////////////////////////////
// BaseDragSource, public:

BaseDragSource::BaseDragSource() : ref_count_(0) {
}

///////////////////////////////////////////////////////////////////////////////
// BaseDragSource, IDropSource implementation:

HRESULT BaseDragSource::QueryContinueDrag(BOOL escape_pressed,
                                          DWORD key_state) {
  if (escape_pressed) {
    OnDragSourceCancel();
    return DRAGDROP_S_CANCEL;
  }

  if (!(key_state & MK_LBUTTON)) {
    OnDragSourceDrop();
    return DRAGDROP_S_DROP;
  }

  OnDragSourceMove();
  return S_OK;
}

HRESULT BaseDragSource::GiveFeedback(DWORD effect) {
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

///////////////////////////////////////////////////////////////////////////////
// BaseDragSource, IUnknown implementation:

HRESULT BaseDragSource::QueryInterface(const IID& iid, void** object) {
  *object = NULL;
  if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDropSource)) {
    *object = this;
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG BaseDragSource::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

ULONG BaseDragSource::Release() {
  if (InterlockedDecrement(&ref_count_) == 0) {
    ULONG copied_refcnt = ref_count_;
    delete this;
    return copied_refcnt;
  }
  return ref_count_;
}
