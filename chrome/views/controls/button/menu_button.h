// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_

#include <windows.h>

#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/background.h"
#include "chrome/views/controls/button/text_button.h"
#include "base/time.h"

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

  // Activate the button (called when the button is pressed).
  virtual bool Activate();

  // Overridden to take into account the potential use of a drop marker.
  virtual gfx::Size GetPreferredSize();
  virtual void Paint(ChromeCanvas* canvas, bool for_drag);

  // These methods are overriden to implement a simple push button
  // behavior
  virtual bool OnMousePressed(const MouseEvent& e);
  void OnMouseReleased(const MouseEvent& e, bool canceled);
  virtual bool OnKeyReleased(const KeyEvent& e);
  virtual void OnMouseExited(const MouseEvent& event);

  // Returns the MSAA default action of the current view. The string returned
  // describes the default action that will occur when executing
  // IAccessible::DoDefaultAction.
  bool GetAccessibleDefaultAction(std::wstring* action);

  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);

  // Returns the MSAA state of the current view. Sets the input VARIANT
  // appropriately, and returns true if a change was performed successfully.
  // Overriden from View.
  virtual bool GetAccessibleState(VARIANT* state);

 protected:
  // true if the menu is currently visible.
  bool menu_visible_;

 private:

  // Compute the maximum X coordinate for the current screen. MenuButtons
  // use this to make sure a menu is never shown off screen.
  int GetMaximumScreenXCoordinate();

  DISALLOW_EVIL_CONSTRUCTORS(MenuButton);

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

  friend class TextButtonBackground;
};

}  // namespace views

#endif  // CHROME_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
