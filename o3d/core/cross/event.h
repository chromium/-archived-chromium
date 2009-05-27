/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the declaration of Event.

#ifndef O3D_CORE_CROSS_EVENT_H_
#define O3D_CORE_CROSS_EVENT_H_

#include <deque>

#include "core/cross/types.h"

namespace o3d {

//  This Event class is used to pass information to JavaScript event handlers.
//  It's the single argument passed to JavaScript for any event that we catch
//  and forward through.
//  See http://dev.w3.org/2006/webapi/DOM-Level-3-Events/html/DOM3-Events.html
//  for what we're trying to emulate.  However, some of the fields are hard to
//  produce, so we skip them for now or make up the different in JavaScript.
class Event {
 public:
  // TODO: Figure out what events to add here; stuff like onmouseout is
  // probably handled by the browser.  For a decent list of HTML5 events, see:
  // http://www.w3schools.com/tags/html5_ref_eventattributes.asp
  enum Type {  // When you add to this, don't forget to add to StringFromType!
    TYPE_INVALID,
    TYPE_CLICK,
    TYPE_DBLCLICK,
    TYPE_MOUSEDOWN,
    TYPE_MOUSEMOVE,
    TYPE_MOUSEUP,
    TYPE_WHEEL,
    TYPE_KEYDOWN,
    TYPE_KEYPRESS,
    TYPE_KEYUP,
    TYPE_RESIZE,  // This we also trigger on the fullscreen transition.
    TYPE_CONTEXTMENU,
    NUM_TYPES
  };
  enum Button {
    BUTTON_LEFT = 0,
    BUTTON_MIDDLE = 1,
    BUTTON_RIGHT = 2,
    BUTTON_4 = 3,
    BUTTON_5 = 4,
    NUM_BUTTONS,
  };
  enum Modifier {
    MODIFIER_ALT = 1,
    MODIFIER_CTRL = 2,
    MODIFIER_SHIFT = 4,
    MODIFIER_META = 8,  // Command on OSX
    MODIFIER_MAX = MODIFIER_META,  // Must update this if you add a modifier.
    MODIFIER_MASK = (MODIFIER_MAX << 1) - 1
  };

  static const char *StringFromType(Type type) {
    switch (type) {
      case TYPE_CLICK:
        return "click";
      case TYPE_DBLCLICK:
        return "dblclick";
      case TYPE_KEYDOWN:
        return "keydown";
      case TYPE_KEYPRESS:
        return "keypress";
      case TYPE_KEYUP:
        return "keyup";
      case TYPE_MOUSEDOWN:
        return "mousedown";
      case TYPE_MOUSEMOVE:
        return "mousemove";
      case TYPE_MOUSEUP:
        return "mouseup";
      case TYPE_WHEEL:
        return "wheel";
      case TYPE_RESIZE:
        return "resize";
      case TYPE_CONTEXTMENU:
        return "contextmenu";
      case TYPE_INVALID:
      default:
        DCHECK(false);
        return "invalid";
    }
  }

  static Type TypeFromString(const char *type_string) {
    for (int i = TYPE_INVALID + 1; i < NUM_TYPES; ++i) {
      if (!strcmp(type_string, StringFromType(static_cast<Type>(i)))) {
        return static_cast<Type>(i);
      }
    }
    return TYPE_INVALID;
  }

  static bool ValidType(Type type) {
    return TYPE_INVALID < type && type < NUM_TYPES;
  }

  Event() :
      type_(TYPE_INVALID),
      button_valid_(false),
      modifier_state_valid_(false),
      key_code_valid_(false),
      char_code_valid_(false),
      position_valid_(false),
      delta_valid_(false),
      size_valid_(false),
      valid_(false) { }
  explicit Event(Type type) :
      type_(type),
      button_valid_(false),
      modifier_state_valid_(false),
      key_code_valid_(false),
      char_code_valid_(false),
      position_valid_(false),
      delta_valid_(false),
      size_valid_(false),
      valid_(true) { }
  Event(const Event& event) {
    if (event.valid()) {
      Copy(event);
    } else {
      valid_ = false;
    }
  }
  Event& operator=(const Event& event) {
    if (event.valid()) {
      Copy(event);
    } else {
      valid_ = false;
    }
    return *this;
  }
  bool operator==(const Event& e) const;
  bool operator!=(const Event& e) const {
    return !(*this == e);
  }


  Type type() const {
    DCHECK(valid_);
    return type_;
  }
  // This is generally used only for overriding a type on a copied event [to
  // create a click from a mouseup, for example].
  void set_type(Type type) {
    DCHECK(valid_);
    type_ = type;
  }
  const char *type_string() const {
    return StringFromType(type());
  }

  // Button is valid on mousedown, mouseup, click, and dblclick.
  // It indicates which button actually caused the event.
  int button() const {
    DCHECK(valid_);
    return button_valid_ ? button_ : 0;
  }
  void set_button(int button) {
    DCHECK(valid_);
    DCHECK(button >= 0 && button < NUM_BUTTONS);
    button_valid_ = true;
    button_ = button;
  }
  bool button_valid() const { return button_valid_; }
  void clear_button() { button_valid_ = false; }

  int modifier_state() const {
    DCHECK(valid_);
    return modifier_state_valid_ ? modifier_state_ : 0;
  }
  void set_modifier_state(int state) {
    DCHECK(valid_);
    DCHECK(!(state & ~MODIFIER_MASK));
    modifier_state_valid_ = true;
    modifier_state_ = state;
  }
  bool modifier_state_valid() const { return modifier_state_valid_; }
  void clear_modifier_state() { modifier_state_valid_ = false; }

  bool ctrl_key() const {
    DCHECK(valid_);
    return modifier_state_valid_ ?
        (modifier_state_ & MODIFIER_CTRL) != 0 : false;
  }
  bool alt_key() const {
    DCHECK(valid_);
    return modifier_state_valid_ ?
        (modifier_state_ & MODIFIER_ALT) != 0 : false;
  }
  bool shift_key() const {
    DCHECK(valid_);
    return modifier_state_valid_ ?
        (modifier_state_ & MODIFIER_SHIFT) != 0 : false;
  }
  bool meta_key() const {
    DCHECK(valid_);
    return modifier_state_valid_ ?
        (modifier_state_ & MODIFIER_META) != 0 : false;
  }

  // Key code is valid on keydown and keyup events only.
  int key_code() const {
    DCHECK(valid_);
    return key_code_valid_ ? key_code_ : 0;
  }
  void set_key_code(int key_code) {
    DCHECK(valid_);
    key_code_valid_ = true;
    key_code_ = key_code;
  }
  bool key_code_valid() const { return key_code_valid_; }
  void clear_key_code() { key_code_valid_ = false; }

  // Key char is valid on keypress events only.
  void set_char_code(int char_code) {
    DCHECK(valid_);
    char_code_valid_ = true;
    char_code_ = char_code;
  }
  int char_code() const {
    DCHECK(valid_);
    return char_code_valid_ ? char_code_ : 0;
  }
  bool char_code_valid() const { return char_code_valid_; }
  void clear_char_code() { char_code_valid_ = false; }

  // Position is valid on mouse events only.
  int x() const {
    DCHECK(valid_);
    return position_valid_ ? x_ : 0;
  }
  int y() const {
    DCHECK(valid_);
    return position_valid_ ? y_ : 0;
  }
  int screen_x() const {
    DCHECK(valid_);
    return position_valid_ ? screen_x_ : 0;
  }
  int screen_y() const {
    DCHECK(valid_);
    return position_valid_ ? screen_y_ : 0;
  }

  // This tells whether or not the position was within the plugin region at the
  // time of the event.  This is used to determine when to synthesize a click
  // event, which only happens when both a mousedown and its corresponding
  // mouseup both occur within the plugin region [regardless of the location
  // intervening mousemove events].  Technically we only absolutely need this
  // for mousedown and mouseup, but the interface is simpler to require it on
  // all positioned events, and it might be useful.
  bool in_plugin() const {
    DCHECK(valid_);
    return position_valid_ ? in_plugin_ : false;
  }
  void set_position(int x, int y, int screen_x, int screen_y, bool inPlugin) {
    DCHECK(valid_);
    position_valid_ = true;
    x_ = x;
    y_ = y;
    screen_x_ = screen_x;
    screen_y_ = screen_y;
    in_plugin_ = inPlugin;
  }
  bool position_valid() const { return position_valid_; }
  void clear_position() { position_valid_ = false; }

  // These are used for mouse scroll events only.  Currently one of the two will
  // always be zero, as some platforms can only detect one axis at a time.
  // We'll want to add a z axis at some point.
  int delta_x() const {
    DCHECK(valid_);
    return delta_valid_ ? delta_x_ : 0;
  }
  int delta_y() const {
    DCHECK(valid_);
    return delta_valid_ ? delta_y_ : 0;
  }
  void set_delta(int delta_x, int delta_y) {
    DCHECK(valid_);
    DCHECK(!delta_x || !delta_y);
    delta_valid_ = true;
    delta_x_ = delta_x;
    delta_y_ = delta_y;
  }
  bool delta_valid() const { return delta_valid_; }
  void clear_delta() { delta_valid_ = false; }

  // Width, height, and fullscreen are valid on resize events only.
  int width() const {
    DCHECK(valid_);
    return size_valid_ ? width_ : 0;
  }
  int height() const {
    DCHECK(valid_);
    return size_valid_ ? height_ : 0;
  }
  bool fullscreen() const {
    DCHECK(valid_);
    return size_valid_ ? fullscreen_ : false;
  }
  void set_size(int width, int height, bool fullscreen) {
    DCHECK(valid_);
    size_valid_ = true;
    width_ = width;
    height_ = height;
    fullscreen_ = fullscreen;
  }
  bool size_valid() const { return size_valid_; }
  void clear_size() { size_valid_ = false; }

  bool valid() const { return valid_; }

 protected:
  void Copy(const Event& event) {
    valid_ = true;
    type_ = event.type();
    if (event.button_valid()) {
      set_button(event.button());
    } else {
      button_valid_ = false;
    }
    if (event.modifier_state_valid()) {
      set_modifier_state(event.modifier_state());
    } else {
      modifier_state_valid_ = false;
    }
    if (event.key_code_valid()) {
      set_key_code(event.key_code());
    } else {
      key_code_valid_ = false;
    }
    if (event.char_code_valid()) {
      set_char_code(event.char_code());
    } else {
      char_code_valid_ = false;
    }
    if (event.position_valid()) {
      set_position(event.x(), event.y(), event.screen_x(), event.screen_y(),
          event.in_plugin());
    } else {
      position_valid_ = false;
    }
    if (event.delta_valid()) {
      set_delta(event.delta_x(), event.delta_y());
    } else {
      delta_valid_ = false;
    }
    if (event.size_valid()) {
      set_size(event.width(), event.height(), event.fullscreen());
    } else {
      size_valid_ = false;
    }
  }

 private:
  Type type_;
  int button_;
  bool button_valid_;
  int modifier_state_;
  bool modifier_state_valid_;
  int key_code_;
  bool key_code_valid_;
  int char_code_;
  bool char_code_valid_;
  int x_, y_;
  int screen_x_, screen_y_;
  bool in_plugin_;
  bool position_valid_;
  int delta_x_, delta_y_;
  bool delta_valid_;
  int width_, height_;
  bool fullscreen_;
  bool size_valid_;
  bool valid_;
};

typedef std::deque<Event> EventQueue;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_EVENT_H_
