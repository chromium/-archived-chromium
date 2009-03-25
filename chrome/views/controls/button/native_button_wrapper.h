// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WRAPPER_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WRAPPER_H_

class ChromeFont;

namespace views {

class Checkbox;
class NativeButton;
class RadioButton;

// A specialization of NativeControlWrapper that hosts a platform-native button.
class NativeButtonWrapper {
 public:
  // Updates the native button's label from the state stored in its associated
  // NativeButton.
  virtual void UpdateLabel() = 0;

  // Updates the native button's label font from the state stored in its
  // associated NativeButton.
  virtual void UpdateFont() = 0;

  // Updates the native button's default state from the state stored in its
  // associated NativeButton.
  virtual void UpdateDefault() = 0;

  // Updates the native button's checked state from the state stored in its
  // associated NativeCheckbox. Valid only for checkboxes and radio buttons.
  virtual void UpdateChecked() {}

  // Shows the pushed state for the button if |pushed| is true.
  virtual void SetPushed(bool pushed) {};

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Sets the focus to the button.
  virtual void SetFocus() = 0;

  // Return the width of the button. Used for fixed size buttons (checkboxes and
  // radio buttons) only.
  static int GetFixedWidth();

  // Creates an appropriate NativeButtonWrapper for the platform.
  static NativeButtonWrapper* CreateNativeButtonWrapper(NativeButton* button);
  static NativeButtonWrapper* CreateCheckboxWrapper(Checkbox* checkbox);
  static NativeButtonWrapper* CreateRadioButtonWrapper(
      RadioButton* radio_button);

};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WRAPPER_H_
