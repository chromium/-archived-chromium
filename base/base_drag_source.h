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

#ifndef BASE_BASE_DRAG_SOURCE_H__
#define BASE_BASE_DRAG_SOURCE_H__

#include <objidl.h>

#include "base/basictypes.h"

///////////////////////////////////////////////////////////////////////////////
//
// BaseDragSource
//
//  A base IDropSource implementation. Handles notifications sent by an active
//  drag-drop operation as the user mouses over other drop targets on their
//  system. This object tells Windows whether or not the drag should continue,
//  and supplies the appropriate cursors.
//
class BaseDragSource : public IDropSource {
 public:
  BaseDragSource();
  virtual ~BaseDragSource() { }

  // IDropSource implementation:
  HRESULT __stdcall QueryContinueDrag(BOOL escape_pressed, DWORD key_state);
  HRESULT __stdcall GiveFeedback(DWORD effect);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 protected:
  virtual void OnDragSourceCancel() { }
  virtual void OnDragSourceDrop() { }
  virtual void OnDragSourceMove() { }

 private:
  LONG ref_count_;

  DISALLOW_EVIL_CONSTRUCTORS(BaseDragSource);
};

#endif  // #ifndef BASE_DRAG_SOURCE_H__
