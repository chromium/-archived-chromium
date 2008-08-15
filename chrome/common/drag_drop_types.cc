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

#include "chrome/common/drag_drop_types.h"

#include <oleidl.h>

int DragDropTypes::DropEffectToDragOperation(
    uint32 effect) {
  int drag_operation = DRAG_NONE;
  if (effect & DROPEFFECT_LINK)
    drag_operation |= DRAG_LINK;
  if (effect & DROPEFFECT_COPY)
    drag_operation |= DRAG_COPY;
  if (effect & DROPEFFECT_MOVE)
    drag_operation |= DRAG_MOVE;
  return drag_operation;
}

uint32 DragDropTypes::DragOperationToDropEffect(int drag_operation) {
  uint32 drop_effect = DROPEFFECT_NONE;
  if (drag_operation & DRAG_LINK)
    drop_effect |= DROPEFFECT_LINK;
  if (drag_operation & DRAG_COPY)
    drop_effect |= DROPEFFECT_COPY;
  if (drag_operation & DRAG_MOVE)
    drop_effect |= DROPEFFECT_MOVE;
  return drop_effect;
}
