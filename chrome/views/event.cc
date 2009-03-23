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

}  // namespace views
 
