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
#include "webkit/glue/glue_util.h"
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
  m_deltaX = e.delta_x;
  m_deltaY = e.delta_y;
  m_isAccepted = false;
  m_granularity = e.scroll_by_page ?
      ScrollByPageWheelEvent : ScrollByLineWheelEvent;
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
    case WebInputEvent::RAW_KEY_DOWN:
      return PlatformKeyboardEvent::RawKeyDown;
    case WebInputEvent::CHAR:
      return PlatformKeyboardEvent::Char;
    default:
      ASSERT_NOT_REACHED();
  }
  return PlatformKeyboardEvent::KeyDown;
}

MakePlatformKeyboardEvent::MakePlatformKeyboardEvent(
    const WebKeyboardEvent& e) {
  m_type = ToPlatformKeyboardEventType(e.type);
  m_text = WebCore::String(e.text);
  m_unmodifiedText = WebCore::String(e.unmodified_text);
  m_keyIdentifier = WebCore::String(e.key_identifier);
  m_autoRepeat = (e.modifiers & WebInputEvent::IS_AUTO_REPEAT) != 0;
  m_windowsVirtualKeyCode = e.windows_key_code;
  m_nativeVirtualKeyCode = e.native_key_code;
  m_isKeypad = (e.modifiers & WebInputEvent::IS_KEYPAD) != 0;
  m_shiftKey = (e.modifiers & WebInputEvent::SHIFT_KEY) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::CTRL_KEY) != 0;
  m_altKey = (e.modifiers & WebInputEvent::ALT_KEY) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::META_KEY) != 0;
  m_isSystemKey = e.system_key;
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
