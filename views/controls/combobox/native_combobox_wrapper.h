// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_WRAPPER_H_
#define VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_WRAPPER_H_

#include "base/gfx/native_widget_types.h"

namespace gfx{
class Size;
}

namespace views {

class Combobox;
class View;

class NativeComboboxWrapper {
 public:
  // Updates the combobox's content from its model.
  virtual void UpdateFromModel() = 0;

  // Updates the displayed selected item from the associated Combobox.
  virtual void UpdateSelectedItem() = 0;

  // Updates the enabled state of the combobox from the associated view.
  virtual void UpdateEnabled() = 0;

  // Gets the selected index.
  virtual int GetSelectedItem() const = 0;

  // Returns true if the Combobox dropdown is open.
  virtual bool IsDropdownOpen() const = 0;

  // Returns the preferred size of the combobox.
  virtual gfx::Size GetPreferredSize() const = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Sets the focus to the button.
  virtual void SetFocus() = 0;

  // Returns a handle to the underlying native view for testing.
  virtual gfx::NativeView GetTestingHandle() const = 0;

  static NativeComboboxWrapper* CreateWrapper(Combobox* combobox);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_WRAPPER_H_
