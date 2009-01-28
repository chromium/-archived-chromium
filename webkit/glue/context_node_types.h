// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_CONTEXT_NODE_TYPES_H__
#define WEBKIT_GLUE_CONTEXT_NODE_TYPES_H__

#include "base/basictypes.h"
#include "base/logging.h"

// The type of node that the user may perform a contextual action on
// in the WebView.
struct ContextNode {
  enum TypeBit {
    // No node is selected
    NONE = 0x0,

    // The top page is selected
    PAGE = 0x1,

    // A subframe page is selected
    FRAME = 0x2,

    // A link is selected
    LINK = 0x4,

    // An image is selected
    IMAGE = 0x8,

    // There is a textual or mixed selection that is selected
    SELECTION = 0x10,

    // An editable element is selected
    EDITABLE = 0x20,

    // A misspelled word is selected
    MISSPELLED_WORD = 0x40,
  };

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

  int32 type;
  ContextNode() : type(NONE) {}
  explicit ContextNode(int32 t) : type(t) {}
};

#endif  // WEBKIT_GLUE_CONTEXT_NODE_TYPES_H__

