// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WRAPPER_H_
#define CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WRAPPER_H_

class ChromeFont;

namespace views {

// An interface implemented by the view that owns the NativeButtonWrapper that
// allows it to be notified when the button is pressed.
class NativeButtonWrapperListener {
 public:
  virtual void ButtonPressed() = 0;
};

// A specialization of NativeControlWrapper that hosts a platform-native button.
class NativeButtonWrapper {
 public:
  // Sets/Gets the button's label.
  virtual void SetLabel(const std::wstring& label) = 0;
  virtual std::wstring GetLabel() const = 0;

  // Sets the font used by this button.
  virtual void SetFont(const ChromeFont& font) = 0;

  // Sets whether or not the button should have the default appearance and
  // action in its setting.
  virtual void SetDefaultButton(bool is_default) = 0;
  virtual bool IsDefaultButton() const = 0;

  // Sets the minimum size of the button from the specified size in native
  // dialog units. The definition of this unit may vary from platform to
  // platform. If the width/height is non-zero, the preferred size of the button
  // will not be less than this value when the dialog units are converted to
  // pixels.
  virtual void SetMinimumSizeInPlatformUnits(const gfx::Size& minimum_size) = 0;

  // Call to have the button ignore its minimum size. Use this if you want
  // buttons narrower than the defined minimum size.
  virtual void SetIgnoreMinimumSize(bool ignore_minimum_size) = 0;

  // Sets/Gets the selected state of button. Valid only for checkboxes and radio
  // buttons.
  virtual void SetSelected(bool is_selected) {}
  virtual bool IsSelected(bool is_selected) { return false; }

  // Shows the hover state for the button if |highlight| is true.
  virtual void SetHighlight(bool highlight) {};

  // Retrieves the views::View that hosts the native control. 
  virtual View* GetView() = 0;

  enum Type {
    TYPE_BUTTON,
    TYPE_CHECKBOX,
    TYPE_RADIOBUTTON
  };

  // Creates an appropriate NativeButtonWrapper for the platform.
  static NativeButtonWrapper* CreateNativeButtonWrapper(
      NativeButtonWrapperListener* listener, Type type);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_WRAPPER_H_
