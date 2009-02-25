// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBINPUTEVENT_H_
#define WEBKIT_GLUE_WEBINPUTEVENT_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <vector>
#ifdef __OBJC__
@class NSEvent;
@class NSView;
#else
class NSEvent;
class NSView;
#endif  // __OBJC__
#elif defined(OS_LINUX)
#include <glib.h>
typedef struct _GdkEventButton GdkEventButton;
typedef struct _GdkEventMotion GdkEventMotion;
typedef struct _GdkEventScroll GdkEventScroll;
typedef struct _GdkEventKey GdkEventKey;
#endif

// The classes defined in this file are intended to be used with WebView's
// HandleInputEvent method.  These event types are cross-platform; however,
// there are platform-specific constructors that accept native UI events.
//
// The fields of these event classes roughly correspond to the fields required
// by WebCore's platform event classes.

// WebInputEvent --------------------------------------------------------------

class WebInputEvent {
 public:
  WebInputEvent() : modifiers(0) { }

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
#if defined(OS_WIN)
  WebMouseEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
#elif defined(OS_MACOSX)
  WebMouseEvent(NSEvent *event, NSView* view);
#elif defined(OS_LINUX)
  explicit WebMouseEvent(const GdkEventButton* event);
  explicit WebMouseEvent(const GdkEventMotion* event);
#endif
};

// WebMouseWheelEvent ---------------------------------------------------------

class WebMouseWheelEvent : public WebMouseEvent {
 public:
  int delta_x;
  int delta_y;

  WebMouseWheelEvent() {}
#if defined(OS_WIN)
  WebMouseWheelEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
#elif defined(OS_MACOSX)
  WebMouseWheelEvent(NSEvent *event, NSView* view);
#elif defined(OS_LINUX)
  explicit WebMouseWheelEvent(const GdkEventScroll* event);
#endif
};

// WebKeyboardEvent -----------------------------------------------------------

class WebKeyboardEvent : public WebInputEvent {
 public:
  // The key_code field is the Windows key code associated with this key event.
  // This sometimes matches the ASCII value of the key (for e.g. a-z) but
  // officially ignores case, and has its own set of codes for control keys as
  // well as other visible letters like punctuation.
  // webkit/port/platform/chromium/KeyboardCodes* is an attempt at defining all
  // of these keys, but it's not all the way there yet.  (E.g., the Windows
  // implementation there just passes through the code from the windows message
  // directly.)
  int key_code;

#if defined(OS_MACOSX)
  // text arrays extracted from the native event. On Mac, there may be
  // multiple keys sent as a single event if the flags don't change.
  std::vector<unsigned short> text;
  std::vector<unsigned short> unmodified_text;
  std::vector<unsigned short> key_identifier;
#elif defined(OS_WIN)
  bool system_key;  // Set if we receive a SYSKEYDOWN/WM_SYSKEYUP message.
  MSG actual_message; // Set to the current keyboard message.
#elif defined(OS_LINUX)
  // The unicode character, if available, corresponding to this key event.
  // TODO(evanm): temporary hack for test_shell.  Ideally we'd either manage
  // to stuff everything into key_code, or make this field shared by all
  // implementations, but this will have to do for now.
  wchar_t text;

  // Stores the GDK keyval and modifier state of the GdkEventKey; this is used
  // to transport these values across IPC so they can be used in the front end
  // code.
  guint gdk_keyval;
  guint gdk_modifier;
#endif

  WebKeyboardEvent() 
      : key_code(0)
#if defined(OS_WIN)
        , system_key(false) {
    memset(&actual_message, 0, sizeof(actual_message));
  }
#else
  {}
#endif

#if defined(OS_WIN)
  WebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
#elif defined(OS_MACOSX)
  WebKeyboardEvent(NSEvent *event);
#elif defined(OS_LINUX)
  explicit WebKeyboardEvent(const GdkEventKey* event);
#endif
};


#endif  // WEBKIT_GLUE_WEBINPUTEVENT_H_
