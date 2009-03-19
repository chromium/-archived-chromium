// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/event.h"

#include "chrome/views/view.h"
#include "webkit/glue/webinputevent.h"

namespace views {

Event::Event(EventType type, int flags)
    : type_(type),
#if defined(OS_WIN)
      time_stamp_(GetTickCount()),
#else
      time_stamp_(0),
#endif
      flags_(flags) {
}

#if defined(OS_WIN)
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
#endif

// static
int Event::ConvertWebInputEventFlags(int web_input_event_flags) {
  int r = 0;
  if (web_input_event_flags & WebInputEvent::SHIFT_KEY)
    r |= EF_SHIFT_DOWN;
  if (web_input_event_flags & WebInputEvent::CTRL_KEY)
    r |= EF_CONTROL_DOWN;
  if (web_input_event_flags & WebInputEvent::ALT_KEY)
    r |= EF_ALT_DOWN;
  return r;
}

LocatedEvent::LocatedEvent(const LocatedEvent& model, View* from, View* to)
    : Event(model),
      location_(model.location_) {
  if (to)
    View::ConvertPointToView(from, to, &location_);
}

MouseEvent::MouseEvent(EventType type,
                       View* from,
                       View* to,
                       const gfx::Point &l,
                       int flags)
    : LocatedEvent(LocatedEvent(type, gfx::Point(l.x(), l.y()), flags),
                                from,
                                to) {
};

MouseEvent::MouseEvent(const MouseEvent& model, View* from, View* to)
    : LocatedEvent(model, from, to) {
}


int KeyEvent::GetKeyStateFlags() const {
#if defined(OS_WIN)
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
#else
  return 0;
#endif
}

#if defined(OS_WIN)
bool KeyEvent::IsExtendedKey() const {
  return (message_flags_ & KF_EXTENDED) == KF_EXTENDED;
}
#endif

}  // namespace views
 
