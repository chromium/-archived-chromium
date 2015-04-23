// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_BUTTON_H_

#include "views/view.h"

namespace views {

class Button;

// An interface implemented by an object to let it know that a button was
// pressed.
class ButtonListener {
 public:
  virtual void ButtonPressed(Button* sender) = 0;
};

// A View representing a button. Depending on the specific type, the button
// could be implemented by a native control or custom rendered.
class Button : public View {
 public:
  virtual ~Button();

  void SetTooltipText(const std::wstring& tooltip_text);

  int tag() const { return tag_; }
  void set_tag(int tag) { tag_ = tag; }

  int mouse_event_flags() const { return mouse_event_flags_; }

  // Overridden from View:
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);
  virtual bool GetAccessibleKeyboardShortcut(std::wstring* shortcut);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual void SetAccessibleKeyboardShortcut(const std::wstring& shortcut);
  virtual void SetAccessibleName(const std::wstring& name);

 protected:
  // Construct the Button with a Listener. The listener can be NULL. This can be
  // true of buttons that don't have a listener - e.g. menubuttons where there's
  // no default action and checkboxes.
  explicit Button(ButtonListener* listener);

  // Cause the button to notify the listener that a click occurred.
  virtual void NotifyClick(int mouse_event_flags);

  // The button's listener. Notified when clicked.
  ButtonListener* listener_;

 private:
  // The text shown in a tooltip.
  std::wstring tooltip_text_;

  // Accessibility data.
  std::wstring accessible_shortcut_;
  std::wstring accessible_name_;

  // The id tag associated with this button. Used to disambiguate buttons in
  // the ButtonListener implementation.
  int tag_;

  // Event flags present when the button was clicked.
  int mouse_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(Button);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_BUTTON_H_
