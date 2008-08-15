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

#ifndef CHROME_VIEWS_EVENT_H__
#define CHROME_VIEWS_EVENT_H__

#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "webkit/glue/window_open_disposition.h"

class OSExchangeData;

namespace ChromeViews {

class View;

////////////////////////////////////////////////////////////////////////////////
//
// Event class
//
// An event encapsulates an input event that can be propagated into view
// hierarchies. An event has a type, some flags and a time stamp.
//
// Each major event type has a corresponding Event subclass.
//
// Events are immutable but support copy
//
////////////////////////////////////////////////////////////////////////////////
class Event {
 public:
  // Event types. (prefixed because of a conflict with windows headers)
  enum EventType { ET_UNKNOWN = 0,
                   ET_MOUSE_PRESSED,
                   ET_MOUSE_DRAGGED,
                   ET_MOUSE_RELEASED,
                   ET_MOUSE_MOVED,
                   ET_MOUSE_ENTERED,
                   ET_MOUSE_EXITED,
                   ET_KEY_PRESSED,
                   ET_KEY_RELEASED,
                   ET_MOUSEWHEEL,
                   ET_DROP_TARGET_EVENT };

  // Event flags currently supported
  enum EventFlags { EF_SHIFT_DOWN         = 1 << 0,
                    EF_CONTROL_DOWN       = 1 << 1,
                    EF_ALT_DOWN           = 1 << 2,
                    EF_LEFT_BUTTON_DOWN   = 1 << 3,
                    EF_MIDDLE_BUTTON_DOWN = 1 << 4,
                    EF_RIGHT_BUTTON_DOWN  = 1 << 5 };

  // Return the event type
  EventType GetType() const {
    return type_;
  }

  // Return the event time stamp in ticks
  int GetTimeStamp() const {
    return time_stamp_;
  }

  // Return the flags
  int GetFlags() const {
    return flags_;
  }

  // Return whether the shift modifier is down
  bool IsShiftDown() const {
    return (flags_ & EF_SHIFT_DOWN) != 0;
  }

  // Return whether the control modifier is down
  bool IsControlDown() const {
    return (flags_ & EF_CONTROL_DOWN) != 0;
  }

  // Return whether the alt modifier is down
  bool IsAltDown() const {
    return (flags_ & EF_ALT_DOWN) != 0;
  }

  // Returns the EventFlags in terms of windows flags.
  int GetWindowsFlags() const;

  // Convert windows flags to ChromeViews::Event flags
  static int ConvertWindowsFlags(uint32 win_flags);

  // Convert WebInputEvent::Modifiers flags to ChromeViews::Event flags.
  // Note that this only deals with keyboard modifiers.
  static int ConvertWebInputEventFlags(int web_input_event_flags);

 protected:
  Event(EventType type, int flags);

  Event(const Event& model)
      : type_(model.GetType()),
        time_stamp_(model.GetTimeStamp()),
        flags_(model.GetFlags()) {
  }

 private:
  void operator=(const Event&);

  EventType type_;
  int time_stamp_;
  int flags_;
};

////////////////////////////////////////////////////////////////////////////////
//
// LocatedEvent class
//
// A generifc event that is used for any events that is located at a specific
// position in the screen.
//
////////////////////////////////////////////////////////////////////////////////
class LocatedEvent : public Event {
 public:
  LocatedEvent(EventType type, const gfx::Point& location, int flags)
      : Event(type, flags),
        location_(location) {
  }

  // Create a new LocatedEvent which is identical to the provided model.
  // If from / to views are provided, the model location will be converted
  // from 'from' coordinate system to 'to' coordinate system
  LocatedEvent(const LocatedEvent& model, View* from, View* to);

  // Returns the X location.
  int GetX() const {
    return location_.x();
  }

  // Returns the Y location.
  int GetY() const {
    return location_.y();
  }

  // Returns the location.
  const gfx::Point& location() const {
    return location_;
  }

 private:
  gfx::Point location_;
};

////////////////////////////////////////////////////////////////////////////////
//
// MouseEvent class
//
// A mouse event is used for any input event related to the mouse.
//
////////////////////////////////////////////////////////////////////////////////
class MouseEvent : public LocatedEvent {
 public:
  // Flags specific to mouse events
  enum MouseEventFlags { EF_IS_DOUBLE_CLICK = 1 << 16 };

  // Create a new mouse event
  MouseEvent(EventType type, int x, int y, int flags)
      : LocatedEvent(type, gfx::Point(x, y), flags) {
  }

  // Create a new mouse event from a type and a point. If from / to views
  // are provided, the point will be converted from 'from' coordinate system to
  // 'to' coordinate system.
  MouseEvent(EventType type,
             View* from,
             View* to,
             const gfx::Point &l,
             int flags);

  // Create a new MouseEvent which is identical to the provided model.
  // If from / to views are provided, the model location will be converted
  // from 'from' coordinate system to 'to' coordinate system
  MouseEvent(const MouseEvent& model, View* from, View* to);

  // Conveniences to quickly test what button is down
  bool IsOnlyLeftMouseButton() const {
    return (GetFlags() & EF_LEFT_BUTTON_DOWN) &&
      !(GetFlags() & (EF_MIDDLE_BUTTON_DOWN | EF_RIGHT_BUTTON_DOWN));
  }

  bool IsLeftMouseButton() const {
    return (GetFlags() & EF_LEFT_BUTTON_DOWN) != 0;
  }

  bool IsOnlyMiddleMouseButton() const {
    return (GetFlags() & EF_MIDDLE_BUTTON_DOWN) &&
      !(GetFlags() & (EF_LEFT_BUTTON_DOWN | EF_RIGHT_BUTTON_DOWN));
  }

  bool IsMiddleMouseButton() const {
    return (GetFlags() & EF_MIDDLE_BUTTON_DOWN) != 0;
  }

  bool IsOnlyRightMouseButton() const {
    return (GetFlags() & EF_RIGHT_BUTTON_DOWN) &&
      !(GetFlags() & (EF_LEFT_BUTTON_DOWN | EF_MIDDLE_BUTTON_DOWN));
  }

  bool IsRightMouseButton() const {
    return (GetFlags() & EF_RIGHT_BUTTON_DOWN) != 0;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(MouseEvent);
};

////////////////////////////////////////////////////////////////////////////////
//
// KeyEvent class
//
// A key event is used for any input event related to the keyboard.
// Note: this event is about key pressed, not typed characters.
//
////////////////////////////////////////////////////////////////////////////////
class KeyEvent : public Event {
 public:
  // Create a new key event
  KeyEvent(EventType type, int ch, int repeat_count, int message_flags)
      : Event(type, GetKeyStateFlags()),
        character_(ch),
        repeat_count_(repeat_count),
        message_flags_(message_flags) {
  }

  int GetCharacter() const {
    return character_;
  }

  bool IsExtendedKey() const;

  int GetRepeatCount() const {
    return repeat_count_;
  }

 private:
  int GetKeyStateFlags() const;

  int character_;
  int repeat_count_;
  int message_flags_;

  DISALLOW_EVIL_CONSTRUCTORS(KeyEvent);
};

////////////////////////////////////////////////////////////////////////////////
//
// MouseWheelEvent class
//
// A MouseWheelEvent is used to propagate mouse wheel user events
//
////////////////////////////////////////////////////////////////////////////////
class MouseWheelEvent : public LocatedEvent {
 public:
  // Create a new key event
  MouseWheelEvent(int offset, int x, int y, int flags)
      : LocatedEvent(ET_MOUSEWHEEL, gfx::Point(x, y), flags),
        offset_(offset) {
  }

  int GetOffset() const {
    return offset_;
  }

 private:
  int offset_;

  DISALLOW_EVIL_CONSTRUCTORS(MouseWheelEvent);
};

////////////////////////////////////////////////////////////////////////////////
//
// DropTargetEvent class
//
// A DropTargetEvent is sent to the view the mouse is over during a drag and
// drop operation.
//
////////////////////////////////////////////////////////////////////////////////
class DropTargetEvent : public LocatedEvent {
 public:
  DropTargetEvent(const OSExchangeData& data,
                  int x,
                  int y,
                  int source_operations)
      : LocatedEvent(ET_DROP_TARGET_EVENT, gfx::Point(x, y), 0),
        data_(data),
        source_operations_(source_operations) {
  }

  // Data associated with the drag/drop session.
  const OSExchangeData& GetData() const { return data_; }

  // Bitmask of supported DragDropTypes::DragOperation by the source.
  int GetSourceOperations() const { return source_operations_; }

 private:
  const OSExchangeData& data_;
  int source_operations_;

  DISALLOW_EVIL_CONSTRUCTORS(DropTargetEvent);
};

}  // namespace ChromeViews

#endif  // CHROME_VIEWS_EVENT_H__
