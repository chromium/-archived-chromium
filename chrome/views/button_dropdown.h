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

#ifndef CHROME_VIEWS_BUTTON_DROPDOWN_H__
#define CHROME_VIEWS_BUTTON_DROPDOWN_H__

#include "base/task.h"
#include "chrome/views/button.h"
#include "chrome/views/menu.h"

class Timer;

namespace ChromeViews {

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown
//
// A button class that when pressed (and held) or pressed (and drag down) will
// display a menu
//
////////////////////////////////////////////////////////////////////////////////
class ButtonDropDown : public Button {
 public:
  explicit ButtonDropDown(Menu::Delegate* menu_delegate);
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
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& e);
  virtual void OnMouseReleased(const ChromeViews::MouseEvent& e,
                               bool canceled);
  virtual bool OnMouseDragged(const ChromeViews::MouseEvent& e);

  // Internal function to show the dropdown menu
  void ShowDropDownMenu(HWND window);

  // Specifies who to delegate populating the menu
  Menu::Delegate* menu_delegate_;

  // Y position of mouse when left mouse button is pressed
  int y_position_on_lbuttondown_;

  // A factory for tasks that show the dropdown context menu for the button.
  ScopedRunnableMethodFactory<ButtonDropDown> show_menu_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(ButtonDropDown);
};

} // namespace

#endif  // CHROME_VIEWS_BUTTON_DROPDOWN_H__
