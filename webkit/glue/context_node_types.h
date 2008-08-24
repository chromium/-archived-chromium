// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CONTEXT_NODE_TYPES_H__
#define WEBKIT_GLUE_CONTEXT_NODE_TYPES_H__

#include "base/basictypes.h"
#include "base/logging.h"

// This class is for scoping only.
class ContextNode {
 public:
  // This enumeration defines the type of node that the user may perform a 
  // contextual action on in the WebView. 
  enum Type {

    // No node is selected
    NONE = 0,

    // The top page is selected
    PAGE = 1,

    // A subframe page is selected
    FRAME = 2,

    // A link is selected
    LINK = 3,

    // An image is selected
    IMAGE = 4,

    // An image that is also a link is selected
    IMAGE_LINK = 5,

    // There is a textual or mixed selection that is selected
    SELECTION = 6,

    // An editiable element is selected
    EDITABLE = 7,

    // A misspelled word is selected
    MISPELLED_WORD = 8
  };

  static Type FromInt(int32 type) {
    return static_cast<Type>(type);
  }

  enum Capability {
    CAN_DO_NONE = 0x0,
    CAN_UNDO = 0x1,
    CAN_REDO = 0x2,
    CAN_CUT = 0x4,
    CAN_COPY = 0x8,
    CAN_PASTE = 0x10,
    CAN_DELETE = 0x20,
    CAN_SELECT_ALL = 0x40,
  };
};

#endif  // WEBKIT_GLUE_CONTEXT_NODE_TYPES_H__

