// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_CUSTOM_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_CUSTOM_BUTTON_H_

#include "app/animation.h"
#include "views/controls/button/button.h"

class ThrobAnimation;

namespace views {

// A button with custom rendering. The common base class of ImageButton and
// TextButton.
class CustomButton : public Button,
                     public AnimationDelegate {
 public:
  virtual ~CustomButton();

  // Possible states
  enum ButtonState {
    BS_NORMAL = 0,
    BS_HOT,
    BS_PUSHED,
    BS_DISABLED,
    BS_COUNT
  };

  // Get/sets the current display state of the button.
  ButtonState state() const { return state_; }
  void SetState(ButtonState state);

  // Starts throbbing. See HoverAnimation for a description of cycles_til_stop.
  void StartThrobbing(int cycles_til_stop);

  // Set how long the hover animation will last for.
  void SetAnimationDuration(int duration);

  // Overridden from View:
  virtual void SetEnabled(bool enabled);
  virtual bool IsEnabled() const;
  virtual bool IsFocusable() const;

  void set_triggerable_event_flags(int triggerable_event_flags) {
    triggerable_event_flags_ = triggerable_event_flags;
  }

  int triggerable_event_flags() const {
    return triggerable_event_flags_;
  }

 protected:
  // Construct the Button with a Listener. See comment for Button's ctor.
  explicit CustomButton(ButtonListener* listener);

  // Returns true if the event is one that can trigger notifying the listener.
  // This implementation returns true if the left mouse button is down.
  virtual bool IsTriggerableEvent(const MouseEvent& e);

  // Overridden from View:
  virtual bool AcceleratorPressed(const Accelerator& accelerator);
  virtual bool OnMousePressed(const MouseEvent& e);
  virtual bool OnMouseDragged(const MouseEvent& e);
  virtual void OnMouseReleased(const MouseEvent& e, bool canceled);
  virtual void OnMouseEntered(const MouseEvent& e);
  virtual void OnMouseMoved(const MouseEvent& e);
  virtual void OnMouseExited(const MouseEvent& e);
  virtual bool OnKeyPressed(const KeyEvent& e);
  virtual bool OnKeyReleased(const KeyEvent& e);
  virtual void OnDragDone();
  virtual void ShowContextMenu(int x, int y, bool is_mouse_gesture);
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);

  // The button state (defined in implementation)
  ButtonState state_;

  // Hover animation.
  scoped_ptr<ThrobAnimation> hover_animation_;

 private:
  // Set / test whether the button is highlighted (in the hover state).
  void SetHighlighted(bool highlighted);
  bool IsHighlighted() const;

  // Returns whether the button is pushed.
  bool IsPushed() const;

  // Should we animate when the state changes? Defaults to true, but false while
  // throbbing.
  bool animate_on_state_change_;

  // Mouse event flags which can trigger button actions.
  int triggerable_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(CustomButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_CUSTOM_BUTTON_H_
