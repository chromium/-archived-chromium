// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class describe a keyboard accelerator (or keyboard shortcut).
// Keyboard accelerators are registered with the FocusManager.
// It has a copy constructor and assignment operator so that it can be copied.
// It also defines the < operator so that it can be used as a key in a std::map.
//

#ifndef CHROME_VIEWS_ACCELERATOR_H_
#define CHROME_VIEWS_ACCELERATOR_H_

#include <string>

#include "chrome/views/event.h"

namespace views {

class Accelerator {
 public:
  Accelerator(int keycode,
              bool shift_pressed, bool ctrl_pressed, bool alt_pressed)
      : key_code_(keycode) {
    modifiers_ = 0;
    if (shift_pressed)
      modifiers_ |= Event::EF_SHIFT_DOWN;
    if (ctrl_pressed)
      modifiers_ |= Event::EF_CONTROL_DOWN;
    if (alt_pressed)
      modifiers_ |= Event::EF_ALT_DOWN;
  }

  Accelerator(const Accelerator& accelerator) {
    key_code_ = accelerator.key_code_;
    modifiers_ = accelerator.modifiers_;
  }

  ~Accelerator() { };

  Accelerator& operator=(const Accelerator& accelerator) {
    if (this != &accelerator) {
      key_code_ = accelerator.key_code_;
      modifiers_ = accelerator.modifiers_;
    }
    return *this;
  }

  // We define the < operator so that the KeyboardShortcut can be used as a key
  // in a std::map.
  bool operator <(const Accelerator& rhs) const {
    if (key_code_ != rhs.key_code_)
      return key_code_ < rhs.key_code_;
    return modifiers_ < rhs.modifiers_;
  }

  bool operator ==(const Accelerator& rhs) const {
    return (key_code_ == rhs.key_code_) && (modifiers_ == rhs.modifiers_);
  }

  bool IsShiftDown() const {
    return (modifiers_ & Event::EF_SHIFT_DOWN) == Event::EF_SHIFT_DOWN;
  }

  bool IsCtrlDown() const {
    return (modifiers_ & Event::EF_CONTROL_DOWN) == Event::EF_CONTROL_DOWN;
  }

  bool IsAltDown() const {
    return (modifiers_ & Event::EF_ALT_DOWN) == Event::EF_ALT_DOWN;
  }

  int GetKeyCode() const {
    return key_code_;
  }

  // Returns a string with the localized shortcut if any.
  std::wstring GetShortcutText() const;

 private:
  // The window keycode (VK_...).
  int key_code_;

  // The state of the Shift/Ctrl/Alt keys (see event.h).
  int modifiers_;
};

// An interface that classes that want to register for keyboard accelerators
// should implement.
class AcceleratorTarget {
 public:
  // This method should return true if the accelerator was processed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) = 0;
};
}

#endif  // CHROME_VIEWS_ACCELERATOR_H_
