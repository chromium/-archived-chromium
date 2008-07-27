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

#ifndef WEBKIT_GLUE_WEBINPUTEVENT_H__
#define WEBKIT_GLUE_WEBINPUTEVENT_H__

#include <windows.h>
#include "base/basictypes.h"

// The classes defined in this file are intended to be used with WebView's
// HandleInputEvent method.  These event types are cross-platform; however,
// there are platform-specific constructors that accept native UI events.
//
// The fields of these event classes roughly correspond to the fields required
// by WebCore's platform event classes.

// WebInputEvent --------------------------------------------------------------

class WebInputEvent {
 public:
  enum Type {
    // WebMouseEvent
    MOUSE_DOWN,
    MOUSE_UP,
    MOUSE_MOVE,
    MOUSE_LEAVE,
    MOUSE_DOUBLE_CLICK,

    // WebMouseWheelEvent
    MOUSE_WHEEL,

    // WebKeyboardEvent
    KEY_DOWN,
    KEY_UP,
    CHAR
  };

  enum Modifiers {
    // modifiers for all events:
    SHIFT_KEY      = 1 << 0,
    CTRL_KEY       = 1 << 1,
    ALT_KEY        = 1 << 2,
    META_KEY       = 1 << 3,

    // modifiers for keyboard events:
    IS_KEYPAD      = 1 << 4,
    IS_AUTO_REPEAT = 1 << 5
  };

  Type type;
  int modifiers;
};

// WebMouseEvent --------------------------------------------------------------

class WebMouseEvent : public WebInputEvent {
 public:
  // These values defined for WebCore::MouseButton
  enum Button {
    BUTTON_NONE = -1,
    BUTTON_LEFT,
    BUTTON_MIDDLE,
    BUTTON_RIGHT
  };

  Button button;
  int x;
  int y;
  int global_x;
  int global_y;
  double timestamp_sec;  // Seconds since epoch.
  int layout_test_click_count;  // Only used during layout tests.

  WebMouseEvent() {}
  WebMouseEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
};

// WebMouseWheelEvent ---------------------------------------------------------

class WebMouseWheelEvent : public WebMouseEvent {
 public:
  int delta_x;
  int delta_y;

  WebMouseWheelEvent() {}
  WebMouseWheelEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
};

// WebKeyboardEvent -----------------------------------------------------------

class WebKeyboardEvent : public WebInputEvent {
 public:
  bool system_key;  // Set if we receive a SYSKEYDOWN/WM_SYSKEYUP message.
  MSG actual_message; // Set to the current keyboard message.
  int key_code;
  int key_data;

  WebKeyboardEvent() 
      : system_key(false),
        key_code(0),
        key_data(0) {
    memset(&actual_message, 0, sizeof(actual_message));
  }

  WebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
};


#endif  // WEBKIT_GLUE_WEBINPUTEVENT_H__
