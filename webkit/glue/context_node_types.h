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
