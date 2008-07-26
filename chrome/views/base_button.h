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

#ifndef CHROME_VIEWS_BASE_BUTTON_H__
#define CHROME_VIEWS_BASE_BUTTON_H__

#include <windows.h>

#include "base/logging.h"
#include "chrome/common/throb_animation.h"
#include "chrome/views/event.h"
#include "chrome/views/view.h"

class OSExchangeData;

namespace ChromeViews {

class MouseEvent;

////////////////////////////////////////////////////////////////////////////////
//
// BaseButton
//
// A base button class that shares common button functionality between various
// specializations.
//
////////////////////////////////////////////////////////////////////////////////
class BaseButton : public View,
                   public AnimationDelegate {
 public:
  // Possible states
  typedef enum ButtonState { BS_NORMAL = 0,
                             BS_HOT = 1,
                             BS_PUSHED = 2,
                             BS_DISABLED = 3};
  static const int kButtonStateCount = 4;

  virtual ~BaseButton();

  // Set / test whether the MenuButton is enabled.
  virtual void SetEnabled(bool f);
  virtual bool IsEnabled() const;

  // Set / test whether the button is hot-tracked.
  void SetHotTracked(bool f);
  bool IsHotTracked() const;

  // Set how long the hover animation will last for.
  void SetAnimationDuration(int duration);

  // Starts throbbing. See HoverAnimation for a description of cycles_til_stop.
  void StartThrobbing(int cycles_til_stop);

  // Returns whether the button is pushed.
  bool IsPushed() const;

  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);
  void SetTooltipText(const std::wstring& tooltip);

  // Overridden from View to take into account the enabled state.
  virtual bool IsFocusable() const;
  virtual bool AcceleratorPressed(const Accelerator& accelerator);

  // Overridden from AnimationDelegate to advance the hover state.
  virtual void AnimationProgressed(const Animation* animation);

  // Returns a string containing the mnemonic, or the keyboard shortcut.
  bool GetAccessibleKeyboardShortcut(std::wstring* shortcut);

  // Returns a brief, identifying string, containing a unique, readable name.
  bool GetAccessibleName(std::wstring* name);

  // Assigns a keyboard shortcut string description.
  void SetAccessibleKeyboardShortcut(const std::wstring& shortcut);

  // Assigns an accessible string name.
  void SetAccessibleName(const std::wstring& name);

  //
  // These methods are overriden to implement a simple push button
  // behavior
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& e);
  virtual bool OnMouseDragged(const ChromeViews::MouseEvent& e);
  virtual void OnMouseReleased(const ChromeViews::MouseEvent& e,
                               bool canceled);
  virtual void OnMouseMoved(const ChromeViews::MouseEvent& e);
  virtual void OnMouseEntered(const ChromeViews::MouseEvent& e);
  virtual void OnMouseExited(const ChromeViews::MouseEvent& e);
  virtual bool OnKeyPressed(const KeyEvent& e);
  virtual bool OnKeyReleased(const KeyEvent& e);

  class ButtonListener {
   public:
    //
    // This is invoked once the button is released use BaseButton::GetTag()
    // to find out which button has been pressed.
    //
    virtual void ButtonPressed(BaseButton* sender) = 0;
  };

  //
  // The the listener, the object that receives a notification when this
  // button is pressed.  tag is any int value to uniquely identify this
  // button.
  virtual void SetListener(ButtonListener *l, int tag);

  //
  // Return the button tag as set by SetListener()
  virtual int GetTag();

  //
  // Cause the button to notify the listener that a click occured.
  virtual void NotifyClick(int mouse_event_flags);

  // Valid when the listener is notified. Contains the event flags from the
  // mouse event, or 0 if not invoked from a mouse event.
  int mouse_event_flags() { return mouse_event_flags_; }

  //
  // Get the state.
  //
  int GetState() const {
    return state_;
  }

  virtual void Paint(ChromeCanvas* canvas);

  // Variant of paint that allows you to specify whether the paint is for a
  // drag operation. This may be used during drag and drop to get a
  // representation of this button suitable for drag and drop.
  virtual void Paint(ChromeCanvas* canvas, bool for_drag);

 protected:
  BaseButton();

  // Returns true if the event is one that can trigger notifying the listener.
  // This implementation returns true if the left mouse button is down.
  virtual bool IsTriggerableEvent(const ChromeViews::MouseEvent& e);

  //
  // Set the state. If the state is different, causes the button
  // to be repainted
  //
  virtual void SetState(ButtonState new_state);

  virtual void OnDragDone();

  // Overriden to reset the state to normal (as long as we're not disabled).
  // This ensures we don't get stuck in a down state if on click our ancestor
  // is removed.
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // tooltip text storage
  std::wstring tooltip_text_;

  // storage of strings needed for accessibility
  std::wstring accessible_shortcut_;
  std::wstring accessible_name_;

  // The button state (defined in implementation)
  int state_;

  // Hover animation.
  scoped_ptr<ThrobAnimation> hover_animation_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(BaseButton);

  // The current listener
  ButtonListener* listener_;

  // tag storage
  int tag_;

  // See description in mouse_event_flags().
  int mouse_event_flags_;

  // Should we animate when the state changes? Defaults to true, but false while
  // throbbing.
  bool animate_on_state_change_;
};

} // namespace

#endif  // CHROME_VIEWS_BASE_BUTTON_H__
