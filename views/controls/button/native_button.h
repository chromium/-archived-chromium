// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_

#include "app/gfx/font.h"
#include "views/controls/button/button.h"
#include "views/controls/button/native_button_wrapper.h"

namespace gfx {
class Font;
}

namespace views {

class NativeButton : public Button {
 public:
  // The button's class name.
  static const char kViewClassName[];

  explicit NativeButton(ButtonListener* listener);
  NativeButton(ButtonListener* listener, const std::wstring& label);
  virtual ~NativeButton();

  // Sets/Gets the text to be used as the button's label.
  void SetLabel(const std::wstring& label);
  std::wstring label() const { return label_; }

  // Sets the font to be used when displaying the button's label.
  void set_font(const gfx::Font& font) { font_ = font; }
  const gfx::Font& font() const { return font_; }

  // Sets/Gets whether or not the button appears and behaves as the default
  // button in its current context.
  void SetIsDefault(bool default_button);
  bool is_default() const { return is_default_; }

  // Sets whether or not the button appears as the default button. This does
  // not make it behave as the default (i.e. no enter key accelerator is
  // registered, use SetIsDefault for that).
  void SetAppearsAsDefault(bool default_button);

  void set_minimum_size(const gfx::Size& minimum_size) {
    minimum_size_ = minimum_size;
  }
  void set_ignore_minimum_size(bool ignore_minimum_size) {
    ignore_minimum_size_ = ignore_minimum_size;
  }

  // Called by the wrapper when the actual wrapped native button was pressed.
  void ButtonPressed();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void SetEnabled(bool flag);
  virtual void Focus();

 protected:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual std::string GetClassName() const;
  virtual bool AcceleratorPressed(const Accelerator& accelerator);

  // Create the button wrapper. Can be overridden by subclass to create a
  // wrapper of a particular type. See NativeButtonWrapper interface for types.
  virtual void CreateWrapper();

  // Sets a border to the button. Override to set a different border or to not
  // set one (the default is 0,8,0,8 for push buttons).
  virtual void InitBorder();

  // The object that actually implements the native button.
  NativeButtonWrapper* native_wrapper_;

 private:
  // The button label.
  std::wstring label_;

  // True if the button is the default button in its context.
  bool is_default_;

  // The font used to render the button label.
  gfx::Font font_;

  // True if the button should ignore the minimum size for the platform. Default
  // is false. Set to true to create narrower buttons.
  bool ignore_minimum_size_;

  // The minimum size of the button from the specified size in native dialog
  // units. The definition of this unit may vary from platform to platform. If
  // the width/height is non-zero, the preferred size of the button will not be
  // less than this value when the dialog units are converted to pixels.
  gfx::Size minimum_size_;

  DISALLOW_COPY_AND_ASSIGN(NativeButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_
