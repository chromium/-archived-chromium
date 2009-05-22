// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/event_conversion.h"

#include "EventNames.h"
#include "FrameView.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "StringImpl.h"  // This is so that the KJS build works
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "Widget.h"

#undef LOG
#include "base/gfx/point.h"
#include "base/logging.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

using namespace WebCore;

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

// MakePlatformMouseEvent -----------------------------------------------------

MakePlatformMouseEvent::MakePlatformMouseEvent(Widget* widget,
                                               const WebMouseEvent& e) {
  // TODO(mpcomplete): widget is always toplevel, unless it's a popup.  We
  // may be able to get rid of this once we abstract popups into a WebKit API.
  m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
  m_globalPosition = IntPoint(e.globalX, e.globalY);
  m_button = static_cast<MouseButton>(e.button);
  m_shiftKey = (e.modifiers & WebInputEvent::ShiftKey) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::ControlKey) != 0;
  m_altKey = (e.modifiers & WebInputEvent::AltKey) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::MetaKey) != 0;
  m_modifierFlags = e.modifiers;
  m_timestamp = e.timeStampSeconds;
  m_clickCount = e.clickCount;

  switch (e.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:  // synthesize a move event
      m_eventType = MouseEventMoved;
      break;

    case WebInputEvent::MouseDown:
      m_eventType = MouseEventPressed;
      break;

    case WebInputEvent::MouseUp:
      m_eventType = MouseEventReleased;
      break;

    default:
      NOTREACHED() << "unexpected mouse event type";
  }
}

// MakePlatformWheelEvent -----------------------------------------------------

MakePlatformWheelEvent::MakePlatformWheelEvent(Widget* widget,
                                               const WebMouseWheelEvent& e) {
  m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
  m_globalPosition = IntPoint(e.globalX, e.globalY);
  m_deltaX = e.deltaX;
  m_deltaY = e.deltaY;
  m_wheelTicksX = e.wheelTicksX;
  m_wheelTicksY = e.wheelTicksY;
  m_isAccepted = false;
  m_granularity = e.scrollByPage ?
      ScrollByPageWheelEvent : ScrollByPixelWheelEvent;
  m_shiftKey = (e.modifiers & WebInputEvent::ShiftKey) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::ControlKey) != 0;
  m_altKey = (e.modifiers & WebInputEvent::AltKey) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::MetaKey) != 0;
}

// MakePlatformKeyboardEvent --------------------------------------------------

static inline const PlatformKeyboardEvent::Type ToPlatformKeyboardEventType(
    WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::KeyUp:
      return PlatformKeyboardEvent::KeyUp;
    case WebInputEvent::KeyDown:
      return PlatformKeyboardEvent::KeyDown;
    case WebInputEvent::RawKeyDown:
      return PlatformKeyboardEvent::RawKeyDown;
    case WebInputEvent::Char:
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
  m_unmodifiedText = WebCore::String(e.unmodifiedText);
  m_keyIdentifier = WebCore::String(e.keyIdentifier);
  m_autoRepeat = (e.modifiers & WebInputEvent::IsAutoRepeat) != 0;
  m_windowsVirtualKeyCode = e.windowsKeyCode;
  m_nativeVirtualKeyCode = e.nativeKeyCode;
  m_isKeypad = (e.modifiers & WebInputEvent::IsKeyPad) != 0;
  m_shiftKey = (e.modifiers & WebInputEvent::ShiftKey) != 0;
  m_ctrlKey = (e.modifiers & WebInputEvent::ControlKey) != 0;
  m_altKey = (e.modifiers & WebInputEvent::AltKey) != 0;
  m_metaKey = (e.modifiers & WebInputEvent::MetaKey) != 0;
  m_isSystemKey = e.isSystemKey;
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

static int GetWebInputModifiers(const WebCore::UIEventWithKeyState& event) {
  int modifiers = 0;
  if (event.ctrlKey())
    modifiers |= WebInputEvent::ControlKey;
  if (event.shiftKey())
    modifiers |= WebInputEvent::ShiftKey;
  if (event.altKey())
    modifiers |= WebInputEvent::AltKey;
  if (event.metaKey())
    modifiers |= WebInputEvent::MetaKey;
  return modifiers;
}


bool ToWebMouseEvent(const WebCore::FrameView& view,
                     const WebCore::MouseEvent& event,
                     WebKit::WebMouseEvent* web_event) {
  if (event.type() == WebCore::eventNames().mousemoveEvent) {
    web_event->type = WebInputEvent::MouseMove;
  } else if (event.type() == WebCore::eventNames().mouseoutEvent) {
    web_event->type = WebInputEvent::MouseLeave;
  } else if (event.type() == WebCore::eventNames().mouseoverEvent) {
    web_event->type = WebInputEvent::MouseEnter;
  } else if (event.type() == WebCore::eventNames().mousedownEvent) {
    web_event->type = WebInputEvent::MouseDown;
  } else if (event.type() == WebCore::eventNames().mouseupEvent) {
    web_event->type = WebInputEvent::MouseUp;
  } else {
    // Skip all other mouse events.
    return false;
  }
  web_event->timeStampSeconds = event.timeStamp() * 1.0e-3;
  switch (event.button()) {
    case WebCore::LeftButton:
      web_event->button = WebMouseEvent::ButtonLeft;
      break;
    case WebCore::MiddleButton:
      web_event->button = WebMouseEvent::ButtonMiddle;
      break;
    case WebCore::RightButton:
      web_event->button = WebMouseEvent::ButtonRight;
      break;
  }
  web_event->modifiers = GetWebInputModifiers(event);
  if (event.buttonDown()) {
    switch (event.button()) {
      case WebCore::LeftButton:
        web_event->modifiers |= WebInputEvent::LeftButtonDown;
        break;
      case WebCore::MiddleButton:
        web_event->modifiers |= WebInputEvent::MiddleButtonDown;
        break;
      case WebCore::RightButton:
        web_event->modifiers |= WebInputEvent::RightButtonDown;
        break;
    }
  }
  WebCore::IntPoint p = view.contentsToWindow(WebCore::IntPoint(event.pageX(),
                                                                event.pageY()));
  web_event->globalX = event.screenX();
  web_event->globalY = event.screenY();
  web_event->windowX = p.x();
  web_event->windowY = p.y();
  web_event->x = event.offsetX();
  web_event->y = event.offsetY();
  return true;
}

bool ToWebKeyboardEvent(const WebCore::KeyboardEvent& event,
                        WebKeyboardEvent* web_event) {
  if (event.type() == WebCore::eventNames().keydownEvent) {
    web_event->type = WebInputEvent::KeyDown;
  } else if (event.type() == WebCore::eventNames().keyupEvent) {
    web_event->type = WebInputEvent::KeyUp;
  } else {
    // Skip all other keyboard events.
    return false;
  }
  web_event->modifiers = GetWebInputModifiers(event);
  web_event->timeStampSeconds = event.timeStamp() * 1.0e-3;
  web_event->windowsKeyCode = event.keyCode();
  web_event->nativeKeyCode = event.keyEvent()->nativeVirtualKeyCode();
  return true;
}
