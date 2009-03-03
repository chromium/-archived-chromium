// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/webinputevent.h"

#include "webkit/glue/event_conversion.h"

#undef LOG
#include "base/logging.h"

static const unsigned long kDefaultScrollLinesPerWheelDelta = 3;

// WebMouseEvent --------------------------------------------------------------

static LPARAM GetRelativeCursorPos(HWND hwnd) {
  POINT pos = {-1, -1};
  GetCursorPos(&pos);
  ScreenToClient(hwnd, &pos);
  return MAKELPARAM(pos.x, pos.y);
}

WebMouseEvent::WebMouseEvent(HWND hwnd, UINT message, WPARAM wparam,
                             LPARAM lparam) {
  switch (message) {
    case WM_MOUSEMOVE:
      type = MOUSE_MOVE;
      if (wparam & MK_LBUTTON)
        button = BUTTON_LEFT;
      else if (wparam & MK_MBUTTON)
        button = BUTTON_MIDDLE;
      else if (wparam & MK_RBUTTON)
        button = BUTTON_MIDDLE;
      else
        button = BUTTON_NONE;
      break;
    case WM_MOUSELEAVE:
      type = MOUSE_LEAVE;
      button = BUTTON_NONE;
      // set the current mouse position (relative to the client area of the
      // current window) since none is specified for this event
      lparam = GetRelativeCursorPos(hwnd);
      break;
    case WM_LBUTTONDOWN:
      type = MOUSE_DOWN;
      button = BUTTON_LEFT;
      break;
    case WM_MBUTTONDOWN:
      type = MOUSE_DOWN;
      button = BUTTON_MIDDLE;
      break;
    case WM_RBUTTONDOWN:
      type = MOUSE_DOWN;
      button = BUTTON_RIGHT;
      break;
    case WM_LBUTTONUP:
      type = MOUSE_UP;
      button = BUTTON_LEFT;
      break;
    case WM_MBUTTONUP:
      type = MOUSE_UP;
      button = BUTTON_MIDDLE;
      break;
    case WM_RBUTTONUP:
      type = MOUSE_UP;
      button = BUTTON_RIGHT;
      break;
    case WM_LBUTTONDBLCLK:
      type = MOUSE_DOUBLE_CLICK;
      button = BUTTON_LEFT;
      break;
    case WM_MBUTTONDBLCLK:
      type = MOUSE_DOUBLE_CLICK;
      button = BUTTON_MIDDLE;
      break;
    case WM_RBUTTONDBLCLK:
      type = MOUSE_DOUBLE_CLICK;
      button = BUTTON_RIGHT;
      break;
    default:
      NOTREACHED() << "unexpected native message";
  }

  // set position fields:

  x = static_cast<short>(LOWORD(lparam));
  y = static_cast<short>(HIWORD(lparam));

  POINT global_point = { x, y };
  ClientToScreen(hwnd, &global_point);

  global_x = global_point.x;
  global_y = global_point.y;
  
  // set modifiers:

  if (wparam & MK_CONTROL)
    modifiers |= CTRL_KEY;
  if (wparam & MK_SHIFT)
    modifiers |= SHIFT_KEY;
  if (GetKeyState(VK_MENU) & 0x8000)
    modifiers |= (ALT_KEY | META_KEY);  // TODO(darin): set META properly

  // TODO(pkasting): http://b/1117926 Instead of using GetTickCount() here, we
  // should use GetMessageTime() on the original Windows message in the browser
  // process, and pass that in the WebMouseEvent.
  timestamp_sec = GetTickCount() / 1000.0;

  layout_test_click_count = 0;
}

// WebMouseWheelEvent ---------------------------------------------------------

WebMouseWheelEvent::WebMouseWheelEvent(HWND hwnd, UINT message, WPARAM wparam,
                                       LPARAM lparam) {
  type = MOUSE_WHEEL;
  button = BUTTON_NONE;

  // Add a simple workaround to scroll multiples units per page.
  // The right fix needs to extend webkit's implementation of
  // wheel events and that's not something we want to do at
  // this time. See bug# 928509
  // TODO(joshia): Implement the right fix for bug# 928509
  const int kPageScroll = 10;  // 10 times wheel scroll 

  UINT key_state = GET_KEYSTATE_WPARAM(wparam);
  int wheel_delta = static_cast<int>(GET_WHEEL_DELTA_WPARAM(wparam));

  typedef SHORT (WINAPI *GetKeyStateFunction)(int key);
  GetKeyStateFunction get_key_state = GetKeyState;

  // Synthesize mousewheel event from a scroll event.
  // This is needed to simulate middle mouse scrolling in some 
  // laptops (Thinkpads)
  if ((WM_VSCROLL == message) || (WM_HSCROLL == message)) {

    POINT cursor_position = {0};
    GetCursorPos(&cursor_position);

    global_x = cursor_position.x;
    global_y = cursor_position.y;

    key_state = 0;

    // Since we are synthesizing the wheel event, we have to
    // use GetAsyncKeyState
    if (GetAsyncKeyState(VK_SHIFT))
      key_state |= MK_SHIFT;

    if (GetAsyncKeyState(VK_CONTROL))
      key_state |= MK_CONTROL;

    switch (LOWORD(wparam)) {
      case SB_LINEUP:    // == SB_LINELEFT
        wheel_delta = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:  // == SB_LINERIGHT
        wheel_delta = -WHEEL_DELTA;
        break;
      case SB_PAGEUP:
        wheel_delta = kPageScroll * WHEEL_DELTA;
        break;
      case SB_PAGEDOWN:
        wheel_delta = -kPageScroll * WHEEL_DELTA;
        break;
      // TODO(joshia): Handle SB_THUMBPOSITION and SB_THUMBTRACK 
      // for compeleteness
      default:
        break;
    }

    // Touchpads (or trackpoints) send the following messages in scrolling
    // horizontally.
    //  * Scrolling left
    //    message == WM_HSCROLL, wparam == SB_LINELEFT (== SB_LINEUP).
    //  * Scrolling right
    //    message == WM_HSCROLL, wparam == SB_LINERIGHT (== SB_LINEDOWN).
    if (WM_HSCROLL == message) {	
      key_state |= MK_SHIFT;	
      wheel_delta = -wheel_delta;	
    }

    // Use GetAsyncKeyState for key state since we are synthesizing 
    // the input
    get_key_state = GetAsyncKeyState;
  } else {
    // TODO(hbono): we should add a new variable which indicates scroll
    // direction and remove this key_state hack.
    if (WM_MOUSEHWHEEL == message)
      key_state |= MK_SHIFT;

    global_x = static_cast<short>(LOWORD(lparam));
    global_y = static_cast<short>(HIWORD(lparam));
  }

  POINT client_point = { global_x, global_y };
  ScreenToClient(hwnd, &client_point);
  x = client_point.x;
  y = client_point.y;

  // compute the scroll delta based on Raymond Chen's algorithm:
  // http://blogs.msdn.com/oldnewthing/archive/2003/08/07/54615.aspx

  static int carryover = 0;
  static HWND last_window = NULL;

  if (hwnd != last_window) {
    last_window = hwnd;
    carryover = 0;
  }

  unsigned long scroll_lines = kDefaultScrollLinesPerWheelDelta;
  SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0);

  int delta_lines = 0;
  if (scroll_lines == WHEEL_PAGESCROLL) {
    scroll_lines = kPageScroll;
  }

  if (scroll_lines == 0) {
      carryover = 0;
  } else {
    const int delta = carryover + wheel_delta;

    // see how many lines we should scroll.  relies on round-toward-zero.
    delta_lines = delta * static_cast<int>(scroll_lines) / WHEEL_DELTA;

    // record the unused portion as the next carryover.
    carryover =
        delta - delta_lines * WHEEL_DELTA / static_cast<int>(scroll_lines);
  }

  // Scroll horizontally if shift is held. WebKit's WebKit/win/WebView.cpp 
  // does the equivalent.
  // TODO(jackson): Support WM_MOUSEHWHEEL = 0x020E event as well. 
  // (Need a mouse with horizontal scrolling capabilities to test it.)
  if (key_state & MK_SHIFT) {
    // Scrolling up should move left, scrolling down should move right
    delta_x = -delta_lines;  
    delta_y = 0;
  } else {
    delta_x = 0;
    delta_y = delta_lines;
  }

  if (key_state & MK_SHIFT)
    modifiers |= SHIFT_KEY;
  if (key_state & MK_CONTROL)
    modifiers |= CTRL_KEY;

  // Get any additional key states needed
  if (get_key_state(VK_MENU) & 0x8000)
    modifiers |= (ALT_KEY | META_KEY);
}

// WebKeyboardEvent -----------------------------------------------------------

bool IsKeyPad(WPARAM wparam, LPARAM lparam) {
  bool keypad = false;
  switch (wparam) {
    case VK_RETURN:
      keypad = (lparam >> 16) & KF_EXTENDED;
      break;
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
      keypad = !((lparam >> 16) & KF_EXTENDED);
      break;
    case VK_NUMLOCK:
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_DIVIDE:
    case VK_MULTIPLY:
    case VK_SUBTRACT:
    case VK_ADD:
    case VK_DECIMAL:
    case VK_CLEAR:
      keypad = true;
      break;
    default:
      keypad = false;
  }

  return keypad;
}

WebKeyboardEvent::WebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
  system_key = false;

  actual_message.hwnd = hwnd;
  actual_message.message = message;
  actual_message.wParam = wparam;
  actual_message.lParam = lparam;

  key_code = static_cast<int>(wparam);

  switch (message) {
    case WM_SYSKEYDOWN:
      system_key = true;
    case WM_KEYDOWN:
      type = KEY_DOWN;
      break;
    case WM_SYSKEYUP:
      system_key = true;
    case WM_KEYUP:
      type = KEY_UP;
      break;
    case WM_IME_CHAR:
      type = CHAR;
      break;
    case WM_SYSCHAR:
      system_key = true;
      type = CHAR;
    case WM_CHAR:
      type = CHAR;
      break;
    default:
      NOTREACHED() << "unexpected native message: " << message;
  }

  if (GetKeyState(VK_SHIFT) & 0x8000)
    modifiers |= SHIFT_KEY;
  if (GetKeyState(VK_CONTROL) & 0x8000)
    modifiers |= CTRL_KEY;
  if (GetKeyState(VK_MENU) & 0x8000)
    modifiers |= (ALT_KEY | META_KEY);

  if (LOWORD(lparam) > 1)
    modifiers |= IS_AUTO_REPEAT;
  if (IsKeyPad(wparam, lparam))
    modifiers |= IS_KEYPAD;
}

