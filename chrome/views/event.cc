// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/views/event.h"

#include "chrome/views/view.h"
#include "webkit/glue/webinputevent.h"

namespace ChromeViews {

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
  // Windows Keyboard messages don't come with control key state as parameters
  // like mouse messages do, so we need to explicitly probe for these key states.
  int flags = 0;
  if (GetKeyState(VK_MENU) & 0x80)
    flags |= Event::EF_ALT_DOWN;
  if (GetKeyState(VK_SHIFT) & 0x80)
    flags |= Event::EF_SHIFT_DOWN;
  if (GetKeyState(VK_CONTROL) & 0x80)
    flags |= Event::EF_CONTROL_DOWN;
  return flags;
}

}
