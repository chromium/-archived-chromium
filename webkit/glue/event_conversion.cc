// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "StringImpl.h"  // This is so that the KJS build works

MSVC_PUSH_WARNING_LEVEL(0);
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Widget.h"
MSVC_POP_WARNING();

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
                                               const WebMouseEvent& e)
#if defined(OS_WIN)
  {
#elif defined(OS_MACOSX)
    : PlatformMouseEvent(e.mac_event.get()) {
#elif defined(OS_LINUX)
    : PlatformMouseEvent() {
#endif
#if defined(OS_WIN) || defined(OS_LINUX)
  // TODO(mpcomplete): widget is always toplevel, unless it's a popup.  We
  // may be able to get rid of this once we abstract popups into a WebKit API.
  m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
#endif
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

    case WebInputEvent::MOUSE_UP:
      m_clickCount = last_click_count_;
      m_eventType = MouseEventReleased;
      break;

    default:
      NOTREACHED() << "unexpected mouse event type";
  }

  if (webkit_glue::IsLayoutTestMode()) {
    m_clickCount = e.layout_test_click_count;
  }
}

// MakePlatformWheelEvent -----------------------------------------------------

MakePlatformWheelEvent::MakePlatformWheelEvent(Widget* widget,
                                               const WebMouseWheelEvent& e)
#if defined(OS_WIN) || defined(OS_LINUX)
  {
#elif defined(OS_MACOSX)
    : PlatformWheelEvent(e.mac_event.get()) {
#endif
#if defined(OS_WIN) || defined(OS_LINUX)
  m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
#endif
  m_globalPosition = IntPoint(e.global_x, e.global_y);
  m_deltaX = static_cast<float>(e.delta_x);
  m_deltaY = static_cast<float>(e.delta_y);
  m_charsToScrollPerDelta = 1;
  m_linesToScrollPerDelta = 1;
  m_pageXScrollMode = false;
  m_pageYScrollMode = false;
  m_isAccepted = false;
  m_shiftKey = (e.modifiers & WebInputEvent::SHIFT_KEY) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::CTRL_KEY) != 0;
  m_altKey = (e.modifiers & WebInputEvent::ALT_KEY) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::META_KEY) != 0;
  m_isContinuous = false;
  m_continuousDeltaX = 0;
  m_continuousDeltaY = 0;
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

#if defined(OS_WIN)
static String GetKeyIdentifierForWindowsKeyCode(unsigned short keyCode) {
  switch (keyCode) {
    case VK_MENU:
      return "Alt";
    case VK_CONTROL:
      return "Control";
    case VK_SHIFT:
      return "Shift";
    case VK_CAPITAL:
      return "CapsLock";
    case VK_LWIN:
    case VK_RWIN:
      return "Win";
    case VK_CLEAR:
      return "Clear";
    case VK_DOWN:
      return "Down";
    // "End"
    case VK_END:
      return "End";
    // "Enter"
    case VK_RETURN:
      return "Enter";
    case VK_EXECUTE:
      return "Execute";
    case VK_F1:
      return "F1";
    case VK_F2:
      return "F2";
    case VK_F3:
      return "F3";
    case VK_F4:
      return "F4";
    case VK_F5:
      return "F5";
    case VK_F6:
      return "F6";
    case VK_F7:
      return "F7";
    case VK_F8:
      return "F8";
    case VK_F9:
      return "F9";
    case VK_F10:
      return "F11";
    case VK_F12:
      return "F12";
    case VK_F13:
      return "F13";
    case VK_F14:
      return "F14";
    case VK_F15:
      return "F15";
    case VK_F16:
      return "F16";
    case VK_F17:
      return "F17";
    case VK_F18:
      return "F18";
    case VK_F19:
      return "F19";
    case VK_F20:
      return "F20";
    case VK_F21:
      return "F21";
    case VK_F22:
      return "F22";
    case VK_F23:
      return "F23";
    case VK_F24:
      return "F24";
    case VK_HELP:
      return "Help";
    case VK_HOME:
      return "Home";
    case VK_INSERT:
      return "Insert";
    case VK_LEFT:
      return "Left";
    case VK_NEXT:
      return "PageDown";
    case VK_PRIOR:
      return "PageUp";
    case VK_PAUSE:
      return "Pause";
    case VK_SNAPSHOT:
      return "PrintScreen";
    case VK_RIGHT:
      return "Right";
    case VK_SCROLL:
      return "Scroll";
    case VK_SELECT:
      return "Select";
    case VK_UP:
      return "Up";
    // Standard says that DEL becomes U+007F.
    case VK_DELETE:
      return "U+007F";
    default:
      return String::format("U+%04X", toupper(keyCode));
  }
}
#else
static String GetKeyIdentifierForWindowsKeyCode(unsigned short keyCode) {
  return String::format("U+%04X", toupper(keyCode));
}
#endif

MakePlatformKeyboardEvent::MakePlatformKeyboardEvent(const WebKeyboardEvent& e)
#if defined(OS_WIN) || defined(OS_LINUX)
  {
#elif defined(OS_MACOSX)
    : PlatformKeyboardEvent(e.mac_event.get()) {
#endif
#if defined(OS_WIN) || defined(OS_LINUX)
  m_type = ToPlatformKeyboardEventType(e.type);
  if (m_type == Char || m_type == KeyDown)
    m_text = m_unmodifiedText = ToSingleCharacterString(e.key_code);
  if (m_type != Char)
    m_keyIdentifier = GetKeyIdentifierForWindowsKeyCode(e.key_code);
  if (m_type == Char || m_type == KeyDown || m_type == KeyUp ||
      m_type == RawKeyDown) {
    m_windowsVirtualKeyCode = e.key_code;
  } else {
    m_windowsVirtualKeyCode = 0;
  }
#endif
  m_autoRepeat = (e.modifiers & WebInputEvent::IS_AUTO_REPEAT) != 0;
  m_isKeypad = (e.modifiers & WebInputEvent::IS_KEYPAD) != 0;
  m_shiftKey = (e.modifiers & WebInputEvent::SHIFT_KEY) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::CTRL_KEY) != 0;
  m_altKey = (e.modifiers & WebInputEvent::ALT_KEY) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::META_KEY) != 0;
#if defined(OS_WIN)
  m_isSystemKey = e.system_key;
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
#if defined(OS_WIN)
    case VK_BACK:
    case VK_ESCAPE:
#endif
      return false;

    default:
      break;
  }
  return true;
}

