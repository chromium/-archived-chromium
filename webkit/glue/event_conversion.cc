// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

#include "KeyboardCodes.h"
#include "StringImpl.h"  // This is so that the KJS build works

MSVC_PUSH_WARNING_LEVEL(0);
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Widget.h"
MSVC_POP_WARNING();

#include "WebKit.h"

#undef LOG
#include "base/gfx/point.h"
#include "base/logging.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webkit_glue.h"

using namespace WebCore;

// MakePlatformMouseEvent -----------------------------------------------------

int MakePlatformMouseEvent::last_click_count_ = 0;
uint32 MakePlatformMouseEvent::last_click_time_ = 0;

MakePlatformMouseEvent::MakePlatformMouseEvent(Widget* widget,
                                               const WebMouseEvent& e) {
  // TODO(mpcomplete): widget is always toplevel, unless it's a popup.  We
  // may be able to get rid of this once we abstract popups into a WebKit API.
  m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
  m_globalPosition = IntPoint(e.global_x, e.global_y);
  m_button = static_cast<MouseButton>(e.button);
  m_shiftKey = (e.modifiers & WebInputEvent::SHIFT_KEY) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::CTRL_KEY) != 0;
  m_altKey = (e.modifiers & WebInputEvent::ALT_KEY) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::META_KEY) != 0;
  m_modifierFlags = e.modifiers;
  m_timestamp = e.timestamp_sec;

  // This differs slightly from the WebKit code in WebKit/win/WebView.cpp where
  // their original code looks buggy.
  static IntPoint last_click_position;
  static MouseButton last_click_button = LeftButton;

  const uint32 current_time = static_cast<uint32>(m_timestamp * 1000);
#if defined(OS_WIN)
  const bool cancel_previous_click =
      (abs(last_click_position.x() - m_position.x()) >
       (GetSystemMetrics(SM_CXDOUBLECLK) / 2)) ||
      (abs(last_click_position.y() - m_position.y()) >
       (GetSystemMetrics(SM_CYDOUBLECLK) / 2)) ||
      ((current_time - last_click_time_) > GetDoubleClickTime());
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  const bool cancel_previous_click = false;
#endif

  switch (e.type) {
    case WebInputEvent::MOUSE_MOVE:
    case WebInputEvent::MOUSE_LEAVE:  // synthesize a move event
      if (cancel_previous_click) {
        last_click_count_ = 0;
        last_click_position = IntPoint();
        last_click_time_ = 0;
      }
      m_clickCount = last_click_count_;
      m_eventType = MouseEventMoved;
      break;

  // TODO(port): make these platform agnostic when we restructure this code.
#if defined(OS_LINUX) || defined(OS_MACOSX)
    case WebInputEvent::MOUSE_DOUBLE_CLICK:
      ++m_clickCount;
      // fall through
    case WebInputEvent::MOUSE_DOWN:
      ++m_clickCount;
      last_click_time_ = current_time;
      last_click_button = m_button;
      m_eventType = MouseEventPressed;
      break;
#else
    case WebInputEvent::MOUSE_DOWN:
    case WebInputEvent::MOUSE_DOUBLE_CLICK:
      if (!cancel_previous_click && (m_button == last_click_button)) {
        ++last_click_count_;
      } else {
        last_click_count_ = 1;
        last_click_position = m_position;
      }
      last_click_time_ = current_time;
      last_click_button = m_button;
      m_clickCount = last_click_count_;
      m_eventType = MouseEventPressed;
      break;
#endif

    case WebInputEvent::MOUSE_UP:
      m_clickCount = last_click_count_;
      m_eventType = MouseEventReleased;
      break;

    default:
      NOTREACHED() << "unexpected mouse event type";
  }

  if (WebKit::layoutTestMode()) {
    m_clickCount = e.layout_test_click_count;
  }
}

// MakePlatformWheelEvent -----------------------------------------------------

MakePlatformWheelEvent::MakePlatformWheelEvent(Widget* widget,
                                               const WebMouseWheelEvent& e) {
  m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
  m_globalPosition = IntPoint(e.global_x, e.global_y);
  m_deltaX = static_cast<float>(e.delta_x);
  m_deltaY = static_cast<float>(e.delta_y);
  m_isAccepted = false;
  m_granularity = ScrollByLineWheelEvent;
  m_shiftKey = (e.modifiers & WebInputEvent::SHIFT_KEY) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::CTRL_KEY) != 0;
  m_altKey = (e.modifiers & WebInputEvent::ALT_KEY) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::META_KEY) != 0;
}

// MakePlatformKeyboardEvent --------------------------------------------------

static inline const PlatformKeyboardEvent::Type ToPlatformKeyboardEventType(
    WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::KEY_UP:
      return PlatformKeyboardEvent::KeyUp;
    case WebInputEvent::KEY_DOWN:
      return PlatformKeyboardEvent::KeyDown;
    case WebInputEvent::CHAR:
      return PlatformKeyboardEvent::Char;
    default:
      ASSERT_NOT_REACHED();
  }
  return PlatformKeyboardEvent::KeyDown;
}

static inline String ToSingleCharacterString(UChar c) {
  return String(&c, 1);
}

#if !defined(OS_MACOSX)
// This function is not used on Mac OS X, and gcc complains.
static String GetKeyIdentifierForWindowsKeyCode(unsigned short keyCode) {
  switch (keyCode) {
    case VKEY_MENU:
      return "Alt";
    case VKEY_CONTROL:
      return "Control";
    case VKEY_SHIFT:
      return "Shift";
    case VKEY_CAPITAL:
      return "CapsLock";
    case VKEY_LWIN:
    case VKEY_RWIN:
      return "Win";
    case VKEY_CLEAR:
      return "Clear";
    case VKEY_DOWN:
      return "Down";
    // "End"
    case VKEY_END:
      return "End";
    // "Enter"
    case VKEY_RETURN:
      return "Enter";
    case VKEY_EXECUTE:
      return "Execute";
    case VKEY_F1:
      return "F1";
    case VKEY_F2:
      return "F2";
    case VKEY_F3:
      return "F3";
    case VKEY_F4:
      return "F4";
    case VKEY_F5:
      return "F5";
    case VKEY_F6:
      return "F6";
    case VKEY_F7:
      return "F7";
    case VKEY_F8:
      return "F8";
    case VKEY_F9:
      return "F9";
    case VKEY_F10:
      return "F11";
    case VKEY_F12:
      return "F12";
    case VKEY_F13:
      return "F13";
    case VKEY_F14:
      return "F14";
    case VKEY_F15:
      return "F15";
    case VKEY_F16:
      return "F16";
    case VKEY_F17:
      return "F17";
    case VKEY_F18:
      return "F18";
    case VKEY_F19:
      return "F19";
    case VKEY_F20:
      return "F20";
    case VKEY_F21:
      return "F21";
    case VKEY_F22:
      return "F22";
    case VKEY_F23:
      return "F23";
    case VKEY_F24:
      return "F24";
    case VKEY_HELP:
      return "Help";
    case VKEY_HOME:
      return "Home";
    case VKEY_INSERT:
      return "Insert";
    case VKEY_LEFT:
      return "Left";
    case VKEY_NEXT:
      return "PageDown";
    case VKEY_PRIOR:
      return "PageUp";
    case VKEY_PAUSE:
      return "Pause";
    case VKEY_SNAPSHOT:
      return "PrintScreen";
    case VKEY_RIGHT:
      return "Right";
    case VKEY_SCROLL:
      return "Scroll";
    case VKEY_SELECT:
      return "Select";
    case VKEY_UP:
      return "Up";
    // Standard says that DEL becomes U+007F.
    case VKEY_DELETE:
      return "U+007F";
    default:
      return String::format("U+%04X", toupper(keyCode));
  }
}
#endif  // !defined(OS_MACOSX)

MakePlatformKeyboardEvent::MakePlatformKeyboardEvent(const WebKeyboardEvent& e)
  {
  m_type = ToPlatformKeyboardEventType(e.type);
  if (m_type == Char || m_type == KeyDown) {
#if defined(OS_MACOSX)
    m_text = &e.text[0];
    m_unmodifiedText = &e.unmodified_text[0];
    m_keyIdentifier = &e.key_identifier[0];

    // Always use 13 for Enter/Return -- we don't want to use AppKit's 
    // different character for Enter.
    if (m_windowsVirtualKeyCode == '\r') {
        m_text = "\r";
        m_unmodifiedText = "\r";
    }

    // The adjustments below are only needed in backward compatibility mode, 
    // but we cannot tell what mode we are in from here.

    // Turn 0x7F into 8, because backspace needs to always be 8.
    if (m_text == "\x7F")
        m_text = "\x8";
    if (m_unmodifiedText == "\x7F")
        m_unmodifiedText = "\x8";
    // Always use 9 for tab -- we don't want to use AppKit's different character for shift-tab.
    if (m_windowsVirtualKeyCode == 9) {
        m_text = "\x9";
        m_unmodifiedText = "\x9";
    }
#elif defined(OS_WIN)
    m_text = m_unmodifiedText = ToSingleCharacterString(e.key_code);
#elif defined(OS_LINUX)
    m_text = m_unmodifiedText = ToSingleCharacterString(e.text);
#endif
  }
#if defined(OS_WIN) || defined(OS_LINUX)
  if (m_type != Char)
    m_keyIdentifier = GetKeyIdentifierForWindowsKeyCode(e.key_code);
#endif
  if (m_type == Char || m_type == KeyDown || m_type == KeyUp ||
      m_type == RawKeyDown) {
    m_windowsVirtualKeyCode = e.key_code;
  } else {
    m_windowsVirtualKeyCode = 0;
  }
  m_autoRepeat = (e.modifiers & WebInputEvent::IS_AUTO_REPEAT) != 0;
  m_isKeypad = (e.modifiers & WebInputEvent::IS_KEYPAD) != 0;
  m_shiftKey = (e.modifiers & WebInputEvent::SHIFT_KEY) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::CTRL_KEY) != 0;
  m_altKey = (e.modifiers & WebInputEvent::ALT_KEY) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::META_KEY) != 0;
#if defined(OS_WIN)
  m_isSystemKey = e.system_key;
// TODO(port): set this field properly for linux and mac.
#elif defined(OS_LINUX)
  m_isSystemKey = m_altKey;
#else
  m_isSystemKey = false;
#endif
}

void MakePlatformKeyboardEvent::SetKeyType(Type type) {
  // According to the behavior of Webkit in Windows platform,
  // we need to convert KeyDown to RawKeydown and Char events
  // See WebKit/WebKit/Win/WebView.cpp
  ASSERT(m_type == KeyDown);
  ASSERT(type == RawKeyDown || type == Char);
  m_type = type;

  if (type == RawKeyDown) {
    m_text = String();
    m_unmodifiedText = String();
  } else {
    m_keyIdentifier = String();
    m_windowsVirtualKeyCode = 0;
  }
}

// Please refer to bug http://b/issue?id=961192, which talks about Webkit
// keyboard event handling changes. It also mentions the list of keys
// which don't have associated character events. 
bool MakePlatformKeyboardEvent::IsCharacterKey() const {
  switch (windowsVirtualKeyCode()) {
    case VKEY_BACK:
    case VKEY_ESCAPE:
      return false;

    default:
      break;
  }
  return true;
}
