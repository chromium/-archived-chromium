// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/event.h"

#include <windows.h>

namespace views {

int Event::GetWindowsFlags() const {
  // TODO: need support for x1/x2.
  int result = 0;
  result |= (flags_ & EF_SHIFT_DOWN) ? MK_SHIFT : 0;
  result |= (flags_ & EF_CONTROL_DOWN) ? MK_CONTROL : 0;
  result |= (flags_ & EF_LEFT_BUTTON_DOWN) ? MK_LBUTTON : 0;
  result |= (flags_ & EF_MIDDLE_BUTTON_DOWN) ? MK_MBUTTON : 0;
  result |= (flags_ & EF_RIGHT_BUTTON_DOWN) ? MK_RBUTTON : 0;
  return result;
}

//static
int Event::ConvertWindowsFlags(UINT win_flags) {
  int r = 0;
  if (win_flags & MK_CONTROL)
    r |= EF_CONTROL_DOWN;
  if (win_flags & MK_SHIFT)
    r |= EF_SHIFT_DOWN;
  if (GetKeyState(VK_MENU) < 0)
    r |= EF_ALT_DOWN;
  if (win_flags & MK_LBUTTON)
    r |= EF_LEFT_BUTTON_DOWN;
  if (win_flags & MK_MBUTTON)
    r |= EF_MIDDLE_BUTTON_DOWN;
  if (win_flags & MK_RBUTTON)
    r |= EF_RIGHT_BUTTON_DOWN;
  return r;
}

KeyEvent::KeyEvent(EventType type, int ch, int repeat_count, int message_flags)
    : Event(type, GetKeyStateFlags()),
      character_(ch),
      repeat_count_(repeat_count),
      message_flags_(message_flags) {
      }

int KeyEvent::GetKeyStateFlags() const {
  // Windows Keyboard messages don't come with control key state as parameters
  // like mouse messages do, so we need to explicitly probe for these key
  // states.
  int flags = 0;
  if (GetKeyState(VK_MENU) & 0x80)
    flags |= Event::EF_ALT_DOWN;
  if (GetKeyState(VK_SHIFT) & 0x80)
    flags |= Event::EF_SHIFT_DOWN;
  if (GetKeyState(VK_CONTROL) & 0x80)
    flags |= Event::EF_CONTROL_DOWN;
  return flags;
}

bool KeyEvent::IsExtendedKey() const {
  return (message_flags_ & KF_EXTENDED) == KF_EXTENDED;
}

}  // namespace views
