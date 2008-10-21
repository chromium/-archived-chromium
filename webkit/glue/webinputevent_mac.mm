// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "config.h"

#include "webkit/glue/webinputevent.h"

#include "webkit/glue/event_conversion.h"

#undef LOG
#include "base/logging.h"

static const unsigned long kDefaultScrollLinesPerWheelDelta = 3;

// WebMouseEvent --------------------------------------------------------------

WebMouseEvent::WebMouseEvent(NSEvent *event) {
  switch ([event type]) {
    case NSMouseExited:
      type = MOUSE_LEAVE;
      button = BUTTON_NONE;
      break;
    case NSLeftMouseDown:
      type = [event clickCount] == 2 ? MOUSE_DOUBLE_CLICK : MOUSE_DOWN;
      button = BUTTON_LEFT;
      break;
    case NSOtherMouseDown:
      type = [event clickCount] == 2 ? MOUSE_DOUBLE_CLICK : MOUSE_DOWN;
      button = BUTTON_MIDDLE;
      break;
    case NSRightMouseDown:
      type = [event clickCount] == 2 ? MOUSE_DOUBLE_CLICK : MOUSE_DOWN;
      button = BUTTON_RIGHT;
      break;
    case NSLeftMouseUp:
      type = MOUSE_UP;
      button = BUTTON_LEFT;
      break;
    case NSOtherMouseUp:
      type = MOUSE_UP;
      button = BUTTON_MIDDLE;
      break;
    case NSRightMouseUp:
      type = MOUSE_UP;
      button = BUTTON_RIGHT;
      break;
    case NSMouseMoved:
    case NSMouseEntered:
      type = MOUSE_MOVE;
      break;
    case NSLeftMouseDragged:
      type = MOUSE_MOVE;
      button = BUTTON_LEFT;
      break;
    case NSOtherMouseDragged:
      type = MOUSE_MOVE;
      button = BUTTON_MIDDLE;
      break;
    case NSRightMouseDragged:
      type = MOUSE_MOVE;
      button = BUTTON_RIGHT;
      break;
    default:
      NOTREACHED() << "unexpected native message";
  }

  // set position fields:
  NSPoint location = [NSEvent mouseLocation];  // global coordinates
  global_x = location.x;
  global_y = location.y;

  location = [event locationInWindow];  // local (to receiving window)
  x = location.x;
  y = location.y;
  
  // set modifiers:

  modifiers = 0;
  if ([event modifierFlags] & NSControlKeyMask)
    modifiers |= CTRL_KEY;
  if ([event modifierFlags] & NSShiftKeyMask)
    modifiers |= SHIFT_KEY;
  if ([event modifierFlags] & NSAlternateKeyMask)
    modifiers |= (ALT_KEY | META_KEY);  // TODO(darin): set META properly

  timestamp_sec = [event timestamp];

  layout_test_click_count = 0;
  
  mac_event = event;  // retains |event|
}

// WebMouseWheelEvent ---------------------------------------------------------

WebMouseWheelEvent::WebMouseWheelEvent(NSEvent *event) {
  type = MOUSE_WHEEL;
  button = BUTTON_NONE;

  NSPoint location = [NSEvent mouseLocation];  // global coordinates
  global_x = location.x;
  global_y = location.y;
  
  location = [event locationInWindow];  // local (to receiving window)
  x = location.x;
  y = location.y;

  int wheel_delta = [event deltaY];
  const int delta_lines = wheel_delta * kDefaultScrollLinesPerWheelDelta;

  // Scroll horizontally if shift is held. WebKit's WebKit/win/WebView.cpp 
  // does the equivalent.
  // TODO(jackson): Support WM_MOUSEHWHEEL = 0x020E event as well. 
  // (Need a mouse with horizontal scrolling capabilities to test it.)
  if ([event modifierFlags] & NSShiftKeyMask) {
    // Scrolling up should move left, scrolling down should move right
    delta_x = -delta_lines;  
    delta_y = 0;
  } else {
    delta_x = 0;
    delta_y = delta_lines;
  }

  modifiers = 0;
  if ([event modifierFlags] & NSControlKeyMask)
    modifiers |= CTRL_KEY;
  if ([event modifierFlags] & NSShiftKeyMask)
    modifiers |= SHIFT_KEY;
  if ([event modifierFlags] & NSAlternateKeyMask)
    modifiers |= ALT_KEY;
  
  mac_event = event;  // retains |event|
}

// WebKeyboardEvent -----------------------------------------------------------

WebKeyboardEvent::WebKeyboardEvent(NSEvent *event) {
  switch ([event type]) {
    case NSKeyDown:
      type = KEY_DOWN;
      break;
    case NSKeyUp:
      type = KEY_UP;
      break;
    default:
      NOTREACHED() << "unexpected native message";
  }

  modifiers = 0;
  if ([event modifierFlags] & NSControlKeyMask)
    modifiers |= CTRL_KEY;
  if ([event modifierFlags] & NSShiftKeyMask)
    modifiers |= SHIFT_KEY;
  if ([event modifierFlags] & NSAlternateKeyMask)
    modifiers |= ALT_KEY;
  
  if ([event isARepeat])
    modifiers |= IS_AUTO_REPEAT;

  key_code = [event keyCode];
  
  mac_event = event;  // retains |event|
}
