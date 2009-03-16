// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_BUTTON_DROPDOWN_H__
#define CHROME_VIEWS_BUTTON_DROPDOWN_H__

#include "base/task.h"
#include "chrome/views/image_button.h"
#include "chrome/views/menu.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown
//
// A button class that when pressed (and held) or pressed (and drag down) will
// display a menu
//
////////////////////////////////////////////////////////////////////////////////
class ButtonDropDown : public ImageButton {
 public:
  ButtonDropDown(ButtonListener* listener, Menu::Delegate* menu_delegate);
  virtual ~ButtonDropDown();

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

 private:
  // Overridden from Button
  virtual bool OnMousePressed(const MouseEvent& e);
  virtual void OnMouseReleased(const MouseEvent& e, bool canceled);
  virtual bool OnMouseDragged(const MouseEvent& e);

  // Overridden from View. Used to display the right-click menu, as triggered
  // by the keyboard, for instance. Using the member function ShowDropDownMenu
  // for the actual display.
  virtual void ShowContextMenu(int x,
                               int y,
                               bool is_mouse_gesture);

  // Internal function to show the dropdown menu
  void ShowDropDownMenu(HWND window);

  // Specifies who to delegate populating the menu
  Menu::Delegate* menu_delegate_;

  // Y position of mouse when left mouse button is pressed
  int y_position_on_lbuttondown_;

  // A factory for tasks that show the dropdown context menu for the button.
  ScopedRunnableMethodFactory<ButtonDropDown> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(ButtonDropDown);
};

}  // namespace views

#endif  // CHROME_VIEWS_BUTTON_DROPDOWN_H__
