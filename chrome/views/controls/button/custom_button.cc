// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/button/custom_button.h"

#include "base/base_drag_source.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/throb_animation.h"

namespace views {

// How long the hover animation takes if uninterrupted.
static const int kHoverFadeDurationMs = 150;

////////////////////////////////////////////////////////////////////////////////
// CustomButton, public:

CustomButton::~CustomButton() {
}

void CustomButton::SetState(ButtonState state) {
  if (state != state_) {
    if (animate_on_state_change_ || !hover_animation_->IsAnimating()) {
      animate_on_state_change_ = true;
      if (state_ == BS_NORMAL && state == BS_HOT) {
        // Button is hovered from a normal state, start hover animation.
        hover_animation_->Show();
      } else if (state_ == BS_HOT && state == BS_NORMAL) {
        // Button is returning to a normal state from hover, start hover
        // fade animation.
        hover_animation_->Hide();
      } else {
        hover_animation_->Stop();
      }
    }

    state_ = state;
    SchedulePaint();
  }
}

void CustomButton::StartThrobbing(int cycles_til_stop) {
  animate_on_state_change_ = false;
  hover_animation_->StartThrobbing(cycles_til_stop);
}

void CustomButton::SetAnimationDuration(int duration) {
  hover_animation_->SetSlideDuration(duration);
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, View overrides:

void CustomButton::SetEnabled(bool enabled) {
  if (enabled && state_ == BS_DISABLED) {
    SetState(BS_NORMAL);
  } else if (!enabled && state_ != BS_DISABLED) {
    SetState(BS_DISABLED);
  }
}

bool CustomButton::IsEnabled() const {
  return state_ != BS_DISABLED;
}

bool CustomButton::IsFocusable() const {
  return (state_ != BS_DISABLED) && View::IsFocusable();
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, protected:

CustomButton::CustomButton(ButtonListener* listener)
    : Button(listener),
      state_(BS_NORMAL),
      animate_on_state_change_(true) {
  hover_animation_.reset(new ThrobAnimation(this));
  hover_animation_->SetSlideDuration(kHoverFadeDurationMs);
}

bool CustomButton::IsTriggerableEvent(const MouseEvent& e) {
  return e.IsLeftMouseButton();
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, View overrides (protected):

bool CustomButton::AcceleratorPressed(const Accelerator& accelerator) {
  if (enabled_) {
    SetState(BS_NORMAL);
    NotifyClick(0);
    return true;
  }
  return false;
}

bool CustomButton::OnMousePressed(const MouseEvent& e) {
  if (state_ != BS_DISABLED) {
    if (IsTriggerableEvent(e) && HitTest(e.location()))
      SetState(BS_PUSHED);
    RequestFocus();
  }
  return true;
}

bool CustomButton::OnMouseDragged(const MouseEvent& e) {
  if (state_ != BS_DISABLED) {
    if (!HitTest(e.location()))
      SetState(BS_NORMAL);
    else if (IsTriggerableEvent(e))
      SetState(BS_PUSHED);
    else
      SetState(BS_HOT);
  }
  return true;
}

void CustomButton::OnMouseReleased(const MouseEvent& e, bool canceled) {
  if (InDrag()) {
    // Starting a drag results in a MouseReleased, we need to ignore it.
    return;
  }

  if (state_ != BS_DISABLED) {
    if (canceled || !HitTest(e.location())) {
      SetState(BS_NORMAL);
    } else {
      SetState(BS_HOT);
      if (IsTriggerableEvent(e)) {
        NotifyClick(e.GetFlags());
        // We may be deleted at this point (by the listener's notification
        // handler) so no more doing anything, just return.
        return;
      }
    }
  }
}

void CustomButton::OnMouseEntered(const MouseEvent& e) {
  if (state_ != BS_DISABLED)
    SetState(BS_HOT);
}

void CustomButton::OnMouseMoved(const MouseEvent& e) {
  if (state_ != BS_DISABLED) {
    if (HitTest(e.location())) {
      SetState(BS_HOT);
    } else {
      SetState(BS_NORMAL);
    }
  }
}

void CustomButton::OnMouseExited(const MouseEvent& e) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  if (state_ != BS_DISABLED && !InDrag())
    SetState(BS_NORMAL);
}

bool CustomButton::OnKeyPressed(const KeyEvent& e) {
  if (state_ != BS_DISABLED) {
    // Space sets button state to pushed. Enter clicks the button. This matches
    // the Windows native behavior of buttons, where Space clicks the button
    // on KeyRelease and Enter clicks the button on KeyPressed.
    if (e.GetCharacter() == VK_SPACE) {
      SetState(BS_PUSHED);
      return true;
    } else if  (e.GetCharacter() == VK_RETURN) {
      SetState(BS_NORMAL);
      NotifyClick(0);
      return true;
    }
  }
  return false;
}

bool CustomButton::OnKeyReleased(const KeyEvent& e) {
  if (state_ != BS_DISABLED) {
    if (e.GetCharacter() == VK_SPACE) {
      SetState(BS_NORMAL);
      NotifyClick(0);
      return true;
    }
  }
  return false;
}

void CustomButton::OnDragDone() {
  SetState(BS_NORMAL);
}

void CustomButton::ShowContextMenu(int x, int y, bool is_mouse_gesture) {
  if (GetContextMenuController()) {
    // We're about to show the context menu. Showing the context menu likely
    // means we won't get a mouse exited and reset state. Reset it now to be
    // sure.
    if (state_ != BS_DISABLED)
      SetState(BS_NORMAL);
    View::ShowContextMenu(x, y, is_mouse_gesture);
  }
}

void CustomButton::ViewHierarchyChanged(bool is_add, View *parent,
                                        View *child) {
  if (!is_add && state_ != BS_DISABLED)
    SetState(BS_NORMAL);
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, AnimationDelegate implementation:

void CustomButton::AnimationProgressed(const Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// CustomButton, private:

void CustomButton::SetHighlighted(bool highlighted) {
  if (highlighted && state_ != BS_DISABLED) {
    SetState(BS_HOT);
  } else if (!highlighted && state_ != BS_DISABLED) {
    SetState(BS_NORMAL);
  }
}

bool CustomButton::IsHighlighted() const {
  return state_ == BS_HOT;
}

bool CustomButton::IsPushed() const {
  return state_ == BS_PUSHED;
}

}  // namespace views
