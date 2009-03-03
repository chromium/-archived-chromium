// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBINPUTEVENT_H_
#define WEBKIT_GLUE_WEBINPUTEVENT_H_

#include "base/basictypes.h"
#include "base/string16.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
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
//
// WARNING! These classes must remain PODs (plain old data). They will be
// "serialized" by shipping their raw bytes across the wire, so they must not
// contain any non-bit-copyable member variables!

// WebInputEvent --------------------------------------------------------------

class WebInputEvent {
 public:
  WebInputEvent() : modifiers(0) { }

  // There are two schemes used for keyboard input. On Windows (and,
  // interestingly enough, on Mac Carbon) there are two events for a keypress.
  // One is a raw keydown, which provides the keycode only. If the app doesn't
  // handle that, then the system runs key translation to create an event
  // containing the generated character and pumps that event. In such a scheme,
  // those two events are translated to RAW_KEY_DOWN and CHAR events
  // respectively. In Cocoa and Gtk, key events contain both the keycode and any
  // translation into actual text. In such a case, WebCore will eventually need
  // to split the events (see disambiguateKeyDownEvent and its callers) but we
  // don't worry about that here. We just use a different type (KEY_DOWN) to
  // indicate this.
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
    RAW_KEY_DOWN,
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

  // Returns true if the WebInputEvent |type| is a keyboard event.
  static bool IsKeyboardEventType(int type) {
    return type == KEY_DOWN || type == KEY_UP || type == CHAR;
  }
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

// Caps on string lengths so we can make them static arrays and keep them PODs.
const size_t kTextLengthCap = 4;
// http://www.w3.org/TR/DOM-Level-3-Events/keyset.html lists the identifiers.
// The longest is 18 characters, so we'll round up to the next multiple of 4.
const size_t kIdentifierLengthCap = 20;

class WebKeyboardEvent : public WebInputEvent {
 public:
  // |windows_key_code| is the Windows key code associated with this key event.
  // Sometimes it's direct from the event (i.e. on Windows), sometimes it's via
  // a mapping function.  If you want a list, see
  // webkit/port/platform/chromium/KeyboardCodes* .
  int windows_key_code;

  // The actual key code genenerated by the platform. The DOM spec runs on
  // Windows-equivalent codes (thus |windows_key_code| above) but it doesn't
  // hurt to have this one around.
  int native_key_code;

  // |text| is the text generated by this keystroke. |unmodified_text| is
  // |text|, but unmodified by an concurrently-held modifiers (except shift).
  // This is useful for working out shortcut keys. Linux and Windows guarantee
  // one character per event. The Mac does not, but in reality that's all it
  // ever gives. We're generous, and cap it a bit longer.
  char16 text[kTextLengthCap];
  char16 unmodified_text[kTextLengthCap];

  // This is a string identifying the key pressed.
  char key_identifier[kIdentifierLengthCap];

  // This identifies whether this event was tagged by the system as being a
  // "system key" event (see
  // http://msdn.microsoft.com/en-us/library/ms646286(VS.85).aspx for details).
  // Other platforms don't have this concept, but it's just easier to leave it
  // always false than ifdef.

  bool system_key;

  // References to the original event.
#if defined(OS_WIN)
  MSG actual_message;  // Set to the current keyboard message. TODO(avi): remove
#endif

  WebKeyboardEvent() : windows_key_code(0),
                       native_key_code(0),
                       system_key(false) {
    memset(&text, 0, sizeof(text));
    memset(&unmodified_text, 0, sizeof(unmodified_text));
    memset(&key_identifier, 0, sizeof(key_identifier));
#if defined(OS_WIN)
    memset(&actual_message, 0, sizeof(actual_message));
#endif
  }

#if defined(OS_WIN)
  explicit WebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam,
                            LPARAM lparam);
#elif defined(OS_MACOSX)
  explicit WebKeyboardEvent(NSEvent *event);
#elif defined(OS_LINUX)
  explicit WebKeyboardEvent(const GdkEventKey* event);
#endif
};

#endif  // WEBKIT_GLUE_WEBINPUTEVENT_H_
