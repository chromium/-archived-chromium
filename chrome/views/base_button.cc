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

#include <atlbase.h>
#include <atlapp.h>

#include "chrome/views/base_button.h"

#include "base/base_drag_source.h"
#include "chrome/browser/drag_utils.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/os_exchange_data.h"

namespace ChromeViews {

// How long the hover animation takes if uninterrupted.
static const int kHoverFadeDurationMs = 150;

////////////////////////////////////////////////////////////////////////////////
//
// BaseButton - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

BaseButton::BaseButton()
    : listener_(NULL),
      tag_(-1),
      state_(BS_NORMAL),
      mouse_event_flags_(0),
      animate_on_state_change_(true) {
  hover_animation_.reset(new ThrobAnimation(this));
  hover_animation_->SetSlideDuration(kHoverFadeDurationMs);
}

BaseButton::~BaseButton() {
}

////////////////////////////////////////////////////////////////////////////////
//
// BaseButton - properties
//
////////////////////////////////////////////////////////////////////////////////

bool BaseButton::IsTriggerableEvent(const ChromeViews::MouseEvent& e) {
  return e.IsLeftMouseButton();
}

void BaseButton::SetState(BaseButton::ButtonState new_state) {
  if (new_state != state_) {
    if (animate_on_state_change_ || !hover_animation_->IsAnimating()) {
      animate_on_state_change_ = true;
      if (state_ == BaseButton::BS_NORMAL && new_state == BaseButton::BS_HOT) {
        // Button is hovered from a normal state, start hover animation.
        hover_animation_->Show();
      } else if (state_ == BaseButton::BS_HOT &&
                 new_state == BaseButton::BS_NORMAL) {
        // Button is returning to a normal state from hover, start hover
        // fade animation.
        hover_animation_->Hide();
      } else {
        hover_animation_->Stop();
      }
    }

    state_ = new_state;
    SchedulePaint();
  }
}

void BaseButton::SetEnabled(bool f) {
  if (f && state_ == BS_DISABLED) {
    SetState(BS_NORMAL);
  } else if (!f && state_ != BS_DISABLED) {
    SetState(BS_DISABLED);
  }
}

void BaseButton::SetAnimationDuration(int duration) {
  hover_animation_->SetSlideDuration(duration);
}

void BaseButton::StartThrobbing(int cycles_til_stop) {
  animate_on_state_change_ = false;
  hover_animation_->StartThrobbing(cycles_til_stop);
}

bool BaseButton::IsEnabled() const {
  return state_ != BS_DISABLED;
}

void BaseButton::SetHotTracked(bool f) {
  if (f && state_ != BS_DISABLED) {
    SetState(BS_HOT);
  } else if (!f && state_ != BS_DISABLED) {
    SetState(BS_NORMAL);
  }
}

bool BaseButton::IsHotTracked() const {
  return state_ == BS_HOT;
}

bool BaseButton::IsPushed() const {
  return state_ == BS_PUSHED;
}

void BaseButton::SetListener(ButtonListener *l, int tag) {
  listener_ = l;
  tag_ = tag;
}

int BaseButton::GetTag() {
  return tag_;
}

bool BaseButton::IsFocusable() const {
  return (state_ != BS_DISABLED) && View::IsFocusable();
}

////////////////////////////////////////////////////////////////////////////////
//
// BaseButton - Tooltips
//
////////////////////////////////////////////////////////////////////////////////

bool BaseButton::GetTooltipText(int x, int y, std::wstring* tooltip) {
  if (!tooltip_text_.empty()) {
    *tooltip = tooltip_text_;
    return true;
  }
  return false;
}

void BaseButton::SetTooltipText(const std::wstring& tooltip) {
  tooltip_text_.assign(tooltip);
  TooltipTextChanged();
}

////////////////////////////////////////////////////////////////////////////////
//
// BaseButton - Events
//
////////////////////////////////////////////////////////////////////////////////

bool BaseButton::OnMousePressed(const ChromeViews::MouseEvent& e) {
  if (state_ != BS_DISABLED) {
    if (IsTriggerableEvent(e) && HitTest(e.GetLocation())) {
      SetState(BS_PUSHED);
    }
    if (IsFocusable())
      RequestFocus();
  }
  return true;
}

bool BaseButton::OnMouseDragged(const ChromeViews::MouseEvent& e) {
  if (state_ != BS_DISABLED) {
    if (!HitTest(e.GetLocation()))
      SetState(BS_NORMAL);
    else if (IsTriggerableEvent(e))
      SetState(BS_PUSHED);
    else
      SetState(BS_HOT);
  }
  return true;
}

void BaseButton::OnMouseReleased(const ChromeViews::MouseEvent& e,
                                 bool canceled) {
  if (InDrag()) {
    // Starting a drag results in a MouseReleased, we need to ignore it.
    return;
  }

  if (state_ != BS_DISABLED) {
    if (canceled || !HitTest(e.GetLocation())) {
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

void BaseButton::OnMouseEntered(const ChromeViews::MouseEvent& e) {
  using namespace ChromeViews;
  if (state_ != BS_DISABLED) {
    SetState(BS_HOT);
  }
}

void BaseButton::OnMouseMoved(const ChromeViews::MouseEvent& e) {
  using namespace ChromeViews;

  if (state_ != BS_DISABLED) {
    if (HitTest(e.GetLocation())) {
      SetState(BS_HOT);
    } else {
      SetState(BS_NORMAL);
    }
  }
}

void BaseButton::OnMouseExited(const ChromeViews::MouseEvent& e) {
  using namespace ChromeViews;
  // Starting a drag results in a MouseExited, we need to ignore it.
  if (state_ != BS_DISABLED && !InDrag()) {
    SetState(BS_NORMAL);
  }
}

void BaseButton::NotifyClick(int mouse_event_flags) {
  mouse_event_flags_ = mouse_event_flags;
  if (listener_ != NULL)
    listener_->ButtonPressed(this);
  // NOTE: don't attempt to reset mouse_event_flags_ as the listener may have
  // deleted us.
}

bool BaseButton::OnKeyPressed(const KeyEvent& e) {
  if (state_ != BS_DISABLED) {
    if ((e.GetCharacter() == L' ') || (e.GetCharacter() == L'\n')) {
      SetState(BS_PUSHED);
      return true;
    }
  }
  return false;
}

bool BaseButton::OnKeyReleased(const KeyEvent& e) {
  if (state_ != BS_DISABLED) {
    if ((e.GetCharacter() == L' ') || (e.GetCharacter() == L'\n')) {
      SetState(BS_NORMAL);
      NotifyClick(0);
      return true;
    }
  }
  return false;
}

bool BaseButton::AcceleratorPressed(const Accelerator& accelerator) {
  if (enabled_) {
    SetState(BS_NORMAL);
    NotifyClick(0);
    return true;
  }
  return false;
}

void BaseButton::AnimationProgressed(const Animation* animation) {
  SchedulePaint();
}

void BaseButton::OnDragDone() {
  SetState(BS_NORMAL);
}

void BaseButton::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (!is_add && state_ != BS_DISABLED)
    SetState(BS_NORMAL);
}

////////////////////////////////////////////////////////////////////////////////
//
// BaseButton - Accessibility
//
////////////////////////////////////////////////////////////////////////////////

bool BaseButton::GetAccessibleKeyboardShortcut(std::wstring* shortcut) {
  if (!accessible_shortcut_.empty()) {
    *shortcut = accessible_shortcut_;
    return true;
  }
  return false;
}

bool BaseButton::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void BaseButton::SetAccessibleKeyboardShortcut(const std::wstring& shortcut) {
  accessible_shortcut_.assign(shortcut);
}

void BaseButton::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

void BaseButton::Paint(ChromeCanvas* canvas) {
  View::Paint(canvas);
}

void BaseButton::Paint(ChromeCanvas* canvas, bool for_drag) {
  Paint(canvas);
}

}  // namespace ChromeViews
