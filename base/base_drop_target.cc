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

#include <windows.h>
#include <shlobj.h>

#include "base/base_drop_target.h"

#include "base/logging.h"

///////////////////////////////////////////////////////////////////////////////

IDropTargetHelper* BaseDropTarget::cached_drop_target_helper_ = NULL;

BaseDropTarget::BaseDropTarget(HWND hwnd)
    : suspend_(false),
      ref_count_(0),
      hwnd_(hwnd) {
  DCHECK(hwnd);
  HRESULT result = RegisterDragDrop(hwnd, this);
}

BaseDropTarget::~BaseDropTarget() {
}

// static
IDropTargetHelper* BaseDropTarget::DropHelper() {
  if (!cached_drop_target_helper_) {
    CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
                     IID_IDropTargetHelper,
                     reinterpret_cast<void**>(&cached_drop_target_helper_));
  }
  return cached_drop_target_helper_;
}

///////////////////////////////////////////////////////////////////////////////
// BaseDropTarget, IDropTarget implementation:

HRESULT BaseDropTarget::DragEnter(IDataObject* data_object,
                                  DWORD key_state,
                                  POINTL cursor_position,
                                  DWORD* effect) {
  // Tell the helper that we entered so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper) {
    drop_helper->DragEnter(GetHWND(), data_object,
                           reinterpret_cast<POINT*>(&cursor_position), *effect);
  }

  // You can't drag and drop within the same HWND.
  if (suspend_) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }
  current_data_object_ = data_object;
  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect = OnDragEnter(current_data_object_, key_state, screen_pt, *effect);
  return S_OK;
}

HRESULT BaseDropTarget::DragOver(DWORD key_state,
                                 POINTL cursor_position,
                                 DWORD* effect) {
  // Tell the helper that we moved over it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper)
    drop_helper->DragOver(reinterpret_cast<POINT*>(&cursor_position), *effect);

  if (suspend_) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }

  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect = OnDragOver(current_data_object_, key_state, screen_pt, *effect);
  return S_OK;
}

HRESULT BaseDropTarget::DragLeave() {
  // Tell the helper that we moved out of it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper)
    drop_helper->DragLeave();

  OnDragLeave(current_data_object_);

  current_data_object_ = NULL;
  return S_OK;
}

HRESULT BaseDropTarget::Drop(IDataObject* data_object,
                             DWORD key_state,
                             POINTL cursor_position,
                             DWORD* effect) {
  // Tell the helper that we dropped onto it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper) {
    drop_helper->Drop(current_data_object_,
                      reinterpret_cast<POINT*>(&cursor_position), *effect);
  }

  if (suspend_) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }

  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect = OnDrop(current_data_object_, key_state, screen_pt, *effect);
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// BaseDropTarget, IUnknown implementation:

HRESULT BaseDropTarget::QueryInterface(const IID& iid, void** object) {
  *object = NULL;
  if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDropTarget)) {
    *object = this;
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG BaseDropTarget::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

ULONG BaseDropTarget::Release() {
  if (InterlockedDecrement(&ref_count_) == 0) {
    ULONG copied_refcnt = ref_count_;
    delete this;
    return copied_refcnt;
  }
  return ref_count_;
}

DWORD BaseDropTarget::OnDragEnter(IDataObject* data_object,
                                  DWORD key_state,
                                  POINT cursor_position,
                                  DWORD effect) {
  return DROPEFFECT_NONE;
}

DWORD BaseDropTarget::OnDragOver(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  return DROPEFFECT_NONE;
}

void BaseDropTarget::OnDragLeave(IDataObject* data_object) {
}

DWORD BaseDropTarget::OnDrop(IDataObject* data_object,
                             DWORD key_state,
                             POINT cursor_position,
                             DWORD effect) {
  return DROPEFFECT_NONE;
}
