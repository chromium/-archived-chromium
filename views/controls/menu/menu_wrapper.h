// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_WRAPPER_H_
#define VIEWS_CONTROLS_MENU_MENU_WRAPPER_H_

#include "base/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace views {

class Menu2;

// An interface that wraps an object that implements a menu.
class MenuWrapper {
 public:
  virtual ~MenuWrapper() {}

  // Runs the menu at the specified point. This may or may not block depending
  // on the platform.
  virtual void RunMenuAt(const gfx::Point& point, int alignment) = 0;

  // Cancels the active menu.
  virtual void CancelMenu() = 0;

  // Called when the model supplying data to this menu has changed, and the menu
  // must be rebuilt.
  virtual void Rebuild() = 0;

  // Called when the states of the items in the menu must be updated from the
  // model.
  virtual void UpdateStates() = 0;

  // Retrieve a native menu handle.
  virtual gfx::NativeMenu GetNativeMenu() const = 0;

  // Creates the appropriate instance of this wrapper for the current platform.
  static MenuWrapper* CreateWrapper(Menu2* menu);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_WRAPPER_H_
