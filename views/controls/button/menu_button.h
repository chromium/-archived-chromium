// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_

#include "app/gfx/font.h"
#include "base/time.h"
#include "views/background.h"
#include "views/controls/button/text_button.h"

namespace views {

class MouseEvent;
class ViewMenuDelegate;


////////////////////////////////////////////////////////////////////////////////
//
// MenuButton
//
//  A button that shows a menu when the left mouse button is pushed
//
////////////////////////////////////////////////////////////////////////////////
class MenuButton : public TextButton {
 public:
  //
  // Create a Button
  MenuButton(ButtonListener* listener,
             const std::wstring& text,
             ViewMenuDelegate* menu_delegate,
             bool show_menu_marker);
  virtual ~MenuButton();

  void set_menu_delegate(ViewMenuDelegate* delegate) {
    menu_delegate_ = delegate;
  }

  // Activate the button (called when the button is pressed).
  virtual bool Activate();

  // Overridden to take into account the potential use of a drop marker.
  virtual gfx::Size GetPreferredSize();
  virtual void Paint(gfx::Canvas* canvas, bool for_drag);

  // These methods are overriden to implement a simple push button
  // behavior
  virtual bool OnMousePressed(const MouseEvent& e);
  void OnMouseReleased(const MouseEvent& e, bool canceled);
  virtual bool OnKeyReleased(const KeyEvent& e);
  virtual void OnMouseExited(const MouseEvent& event);

  // Accessibility accessors, overridden from View.
  virtual bool GetAccessibleDefaultAction(std::wstring* action);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual bool GetAccessibleState(AccessibilityTypes::State* state);

 protected:
  // True if the menu is currently visible.
  bool menu_visible_;

 private:
  friend class TextButtonBackground;

  // Compute the maximum X coordinate for the current screen. MenuButtons
  // use this to make sure a menu is never shown off screen.
  int GetMaximumScreenXCoordinate();

  // We use a time object in order to keep track of when the menu was closed.
  // The time is used for simulating menu behavior for the menu button; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in Windows,
  // the button is not part of the displayed menu.
  base::Time menu_closed_time_;

  // The associated menu's resource identifier.
  ViewMenuDelegate* menu_delegate_;

  // Whether or not we're showing a drop marker.
  bool show_menu_marker_;

  DISALLOW_COPY_AND_ASSIGN(MenuButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
