// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/webinputevent.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/glue/webinputevent_util.h"

static const unsigned long kDefaultScrollLinesPerWheelDelta = 3;
static const unsigned long kDefaultScrollCharsPerWheelDelta = 1;

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

WebMouseWheelEvent::WebMouseWheelEvent(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam)
    : scroll_by_page(false) {
  type = MOUSE_WHEEL;
  button = BUTTON_NONE;

  // Get key state, coordinates, and wheel delta from event.
  typedef SHORT (WINAPI *GetKeyStateFunction)(int key);
  GetKeyStateFunction get_key_state;
  UINT key_state;
  float wheel_delta;
  bool horizontal_scroll = false;
  if ((message == WM_VSCROLL) || (message == WM_HSCROLL)) {
    // Synthesize mousewheel event from a scroll event.  This is needed to
    // simulate middle mouse scrolling in some laptops.  Use GetAsyncKeyState
    // for key state since we are synthesizing the input event.
    get_key_state = GetAsyncKeyState;
    key_state = 0;
    if (get_key_state(VK_SHIFT))
      key_state |= MK_SHIFT;
    if (get_key_state(VK_CONTROL))
      key_state |= MK_CONTROL;

    POINT cursor_position = {0};
    GetCursorPos(&cursor_position);
    global_x = cursor_position.x;
    global_y = cursor_position.y;

    switch (LOWORD(wparam)) {
      case SB_LINEUP:    // == SB_LINELEFT
        wheel_delta = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:  // == SB_LINERIGHT
        wheel_delta = -WHEEL_DELTA;
        break;
      case SB_PAGEUP:
        wheel_delta = 1;
        scroll_by_page = true;
        break;
      case SB_PAGEDOWN:
        wheel_delta = -1;
        scroll_by_page = true;
        break;
      default:  // We don't supoprt SB_THUMBPOSITION or SB_THUMBTRACK here.
        wheel_delta = 0;
        break;
    }

    if (message == WM_HSCROLL) {
      horizontal_scroll = true;
      wheel_delta = -wheel_delta;
    }
  } else {
    // Non-synthesized event; we can just read data off the event.
    get_key_state = GetKeyState;
    key_state = GET_KEYSTATE_WPARAM(wparam);

    global_x = static_cast<short>(LOWORD(lparam));
    global_y = static_cast<short>(HIWORD(lparam));

    wheel_delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam));
    if (((message == WM_MOUSEHWHEEL) || (key_state & MK_SHIFT)) &&
        (wheel_delta != 0))
      horizontal_scroll = true;
  }

  // Set modifiers based on key state.
  if (key_state & MK_SHIFT)
    modifiers |= SHIFT_KEY;
  if (key_state & MK_CONTROL)
    modifiers |= CTRL_KEY;
  if (get_key_state(VK_MENU) & 0x8000)
    modifiers |= (ALT_KEY | META_KEY);

  // Set coordinates by translating event coordinates from screen to client.
  POINT client_point = { global_x, global_y };
  MapWindowPoints(NULL, hwnd, &client_point, 1);
  x = client_point.x;
  y = client_point.y;

  // Convert wheel delta amount to a number of pixels to scroll.
  //
  // How many pixels should we scroll per line?  Gecko uses the height of the
  // current line, which means scroll distance changes as you go through the
  // page or go to different pages.  IE 7 is ~50 px/line, although the value
  // seems to vary slightly by page and zoom level.  Since IE 7 has a smoothing
  // algorithm on scrolling, it can get away with slightly larger scroll values
  // without feeling jerky.  Here we use 100 px per three lines (the default
  // scroll amount is three lines per wheel tick).
  static const float kScrollbarPixelsPerLine = 100.0f / 3.0f;
  float scroll_delta = wheel_delta / WHEEL_DELTA;
  if (horizontal_scroll) {
    unsigned long scroll_chars = kDefaultScrollCharsPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scroll_chars, 0);
    // TODO(pkasting): Should probably have a different multiplier
    // kScrollbarPixelsPerChar here.
    scroll_delta *= static_cast<float>(scroll_chars) * kScrollbarPixelsPerLine;
  } else {
    unsigned long scroll_lines = kDefaultScrollLinesPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0);
    if (scroll_lines == WHEEL_PAGESCROLL)
      scroll_by_page = true;
    if (!scroll_by_page) {
      scroll_delta *=
          static_cast<float>(scroll_lines) * kScrollbarPixelsPerLine;
    }
  }

  // Set scroll amount based on above calculations.
  if (horizontal_scroll) {
    delta_x = scroll_delta;
    delta_y = 0;
  } else {
    delta_x = 0;
    delta_y = scroll_delta;
  }
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

  windows_key_code = native_key_code = static_cast<int>(wparam);

  switch (message) {
    case WM_SYSKEYDOWN:
      system_key = true;
    case WM_KEYDOWN:
      type = RAW_KEY_DOWN;
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

  memset(&text, 0, sizeof(text));
  memset(&unmodified_text, 0, sizeof(unmodified_text));
  memset(&key_identifier, 0, sizeof(key_identifier));

  if (type == CHAR || type == RAW_KEY_DOWN) {
    text[0] = windows_key_code;
    unmodified_text[0] = windows_key_code;
  }
  if (type != CHAR) {
    std::string key_identifier_str =
        webkit_glue::GetKeyIdentifierForWindowsKeyCode(windows_key_code);
    base::strlcpy(key_identifier, key_identifier_str.c_str(),
                  kIdentifierLengthCap);
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

