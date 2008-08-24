// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

