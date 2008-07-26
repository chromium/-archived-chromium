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

// This class describe a keyboard accelerator (or keyboard shortcut).
// Keyboard accelerators are registered with the FocusManager.
// It has a copy constructor and assignment operator so that it can be copied.
// It also defines the < operator so that it can be used as a key in a std::map.
//

#ifndef CHROME_VIEWS_ACCELERATOR_H__
#define CHROME_VIEWS_ACCELERATOR_H__

#include "chrome/views/event.h"

namespace ChromeViews {

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

  Accelerator& Accelerator::operator=(const Accelerator& accelerator) {
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

#endif  // #ifndef CHROME_VIEWS_ACCELERATOR_H__