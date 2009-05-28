// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WRAPPER_H_
#define VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WRAPPER_H_

#include "base/gfx/native_widget_types.h"

namespace views {

class Textfield;
class View;

// An interface implemented by an object that provides a platform-native
// text field.
class NativeTextfieldWrapper {
 public:
  // The Textfield calls this when it is destroyed to clean up the wrapper
  // object.
  virtual ~NativeTextfieldWrapper() {}

  // Gets the text displayed in the wrapped native text field.
  virtual std::wstring GetText() const = 0;
  
  // Updates the text displayed with the text held by the Textfield.
  virtual void UpdateText() = 0;

  // Adds the specified text to the text already displayed by the wrapped native
  // text field.
  virtual void AppendText(const std::wstring& text) = 0;

  // Gets the text that is selected in the wrapped native text field.
  virtual std::wstring GetSelectedText() const = 0;

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  virtual void SelectAll() = 0;

  // Clears the selection within the edit field and sets the caret to the end.
  virtual void ClearSelection() = 0;

  // Updates the border display for the native text field with the state desired
  // by the Textfield.
  virtual void UpdateBorder() = 0;

  // Updates the background color used when painting the native text field.
  virtual void UpdateBackgroundColor() = 0;

  // Updates the read-only state of the native text field.
  virtual void UpdateReadOnly() = 0;

  // Updates the font used to render text in the native text field.
  virtual void UpdateFont() = 0;

  // Updates the enabled state of the native text field.
  virtual void UpdateEnabled() = 0;

  // Sets the horizontal margins for the native text field.
  virtual void SetHorizontalMargins(int left, int right) = 0;

  // Sets the focus to the text field.
  virtual void SetFocus() = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Returns a handle to the underlying native view for testing.
  virtual gfx::NativeView GetTestingHandle() const = 0;

  // Creates an appropriate NativeTextfieldWrapper for the platform.
  static NativeTextfieldWrapper* CreateWrapper(Textfield* field);
};

}  // namespace views

#endif  // #ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WRAPPER_H_
