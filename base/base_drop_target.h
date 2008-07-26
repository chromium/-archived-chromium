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

#ifndef BASE_BASE_DROP_TARGET_H__
#define BASE_BASE_DROP_TARGET_H__

#include <atlbase.h>
#include <objidl.h>
#include <shobjidl.h>

#include "base/basictypes.h"

// A DropTarget implementation that takes care of the nitty gritty
// of dnd. While this class is concrete, subclasses will most likely
// want to override various OnXXX methods.
//
// Because BaseDropTarget is ref counted you shouldn't delete it directly,
// rather wrap it in a scoped_refptr. Be sure and invoke RevokeDragDrop(m_hWnd)
// before the HWND is deleted too.
class BaseDropTarget : public IDropTarget {
 public:
  // Create a new BaseDropTarget associating it with the given HWND.
  explicit BaseDropTarget(HWND hwnd);
  virtual ~BaseDropTarget();

  // When suspend is set to |true|, the drop target does not receive drops from
  // drags initiated within the owning HWND.
  // TODO(beng): (http://b/1085385) figure out how we will handle legitimate
  //             drag-drop operations within the same HWND, such as dragging
  //             selected text to an edit field.
  void set_suspend(bool suspend) { suspend_ = suspend; }

  // IDropTarget implementation:
  HRESULT __stdcall DragEnter(IDataObject* data_object,
                              DWORD key_state,
                              POINTL cursor_position,
                              DWORD* effect);
  HRESULT __stdcall DragOver(DWORD key_state,
                             POINTL cursor_position,
                             DWORD* effect);
  HRESULT __stdcall DragLeave();
  HRESULT __stdcall Drop(IDataObject* data_object,
                         DWORD key_state,
                         POINTL cursor_position,
                         DWORD* effect);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 protected:
  // Returns the hosting HWND.
  HWND GetHWND() { return hwnd_; }

  // Invoked when the cursor first moves over the hwnd during a dnd session.
  // This should return a bitmask of the supported drop operations:
  // DROPEFFECT_NONE, DROPEFFECT_COPY, DROPEFFECT_LINK and/or
  // DROPEFFECT_MOVE.
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);

  // Invoked when the cursor moves over the window during a dnd session.
  // This should return a bitmask of the supported drop operations:
  // DROPEFFECT_NONE, DROPEFFECT_COPY, DROPEFFECT_LINK and/or
  // DROPEFFECT_MOVE.
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);

  // Invoked when the cursor moves outside the bounds of the hwnd during a
  // dnd session.
  virtual void OnDragLeave(IDataObject* data_object);

  // Invoked when the drop ends on the window. This should return the operation
  // that was taken.
  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // Returns the cached drop helper, creating one if necessary. The returned
  // object is not addrefed. May return NULL if the object couldn't be created.
  static IDropTargetHelper* DropHelper();

  // The data object currently being dragged over this drop target.
  CComPtr<IDataObject> current_data_object_;

  // A helper object that is used to provide drag image support while the mouse
  // is dragging over the content area.
  //
  // DO NOT ACCESS DIRECTLY! Use DropHelper() instead, which will lazily create
  // this if it doesn't exist yet. This object can take tens of milliseconds to
  // create, and we don't want to block any window opening for this, especially
  // since often, DnD will never be used. Instead, we force this penalty to the
  // first time it is actually used.
  static IDropTargetHelper* cached_drop_target_helper_;

  // The HWND of the source. This HWND is used to determine coordinates for
  // mouse events that are sent to the renderer notifying various drag states.
  HWND hwnd_;

  // Whether or not we are currently processing drag notifications for drags
  // initiated in this window.
  bool suspend_;

  LONG ref_count_;

  DISALLOW_EVIL_CONSTRUCTORS(BaseDropTarget);
};

#endif  // BASE_BASE_DROP_TARGET_H__
