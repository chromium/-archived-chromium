// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_DRAG_DROP_TYPES_H_
#define APP_DRAG_DROP_TYPES_H_

#include "base/basictypes.h"

class DragDropTypes {
 public:
  enum DragOperation {
    DRAG_NONE = 0,
    DRAG_MOVE = 1 << 0,
    DRAG_COPY = 1 << 1,
    DRAG_LINK = 1 << 2
  };

  static uint32 DragOperationToDropEffect(int drag_operation);
  static int DropEffectToDragOperation(uint32 effect);
};

#endif  // APP_DRAG_DROP_TYPES_H_
