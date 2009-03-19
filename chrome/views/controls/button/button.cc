// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/button/button.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Button, public:

Button::~Button() {
}

void Button::SetTooltipText(const std::wstring& tooltip_text) {
  tooltip_text_ = tooltip_text;
  TooltipTextChanged();
}

////////////////////////////////////////////////////////////////////////////////
// Button, View overrides:

bool Button::GetTooltipText(int x, int y, std::wstring* tooltip) {
  if (!tooltip_text_.empty()) {
    *tooltip = tooltip_text_;
    return true;
  }
  return false;
}

bool Button::GetAccessibleKeyboardShortcut(std::wstring* shortcut) {
  if (!accessible_shortcut_.empty()) {
    *shortcut = accessible_shortcut_;
    return true;
  }
  return false;
}

bool Button::GetAccessibleName(std::wstring* name) {
  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void Button::SetAccessibleKeyboardShortcut(const std::wstring& shortcut) {
  accessible_shortcut_.assign(shortcut);
}

void Button::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

////////////////////////////////////////////////////////////////////////////////
// Button, protected:

Button::Button(ButtonListener* listener)
    : listener_(listener),
      tag_(-1),
      mouse_event_flags_(0) {
}

void Button::NotifyClick(int mouse_event_flags) {
  mouse_event_flags_ = mouse_event_flags;
  // We can be called when there is no listener, in cases like double clicks on
  // menu buttons etc.
  if (listener_)
    listener_->ButtonPressed(this);
  // NOTE: don't attempt to reset mouse_event_flags_ as the listener may have
  // deleted us.
}

}  // namespace views
