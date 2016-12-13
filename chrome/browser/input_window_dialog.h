// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INPUT_WINDOW_DIALOG_H_
#define CHROME_BROWSER_INPUT_WINDOW_DIALOG_H_

#include <string>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"

// Cross platform access to a modal input window.
class InputWindowDialog {
 public:
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Checks whether |text| is a valid input string.
    virtual bool IsValid(const std::wstring& text) = 0;

    // Callback for when the user clicks the OK button.
    virtual void InputAccepted(const std::wstring& text) = 0;

    // Callback for when the user clicks the Cancel button.
    virtual void InputCanceled() = 0;
  };

  // Creates a new input window dialog parented to |parent|. Ownership of
  // |delegate| is taken by InputWindowDialog or InputWindowDialog's owner.
  static InputWindowDialog* Create(gfx::NativeView parent,
                                   const std::wstring& window_title,
                                   const std::wstring& label,
                                   const std::wstring& contents,
                                   Delegate* delegate);

  // Displays the window.
  virtual void Show() = 0;

  // Closes the window.
  virtual void Close() = 0;

 protected:
  InputWindowDialog() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputWindowDialog);
};

#endif // CHROME_BROWSER_INPUT_WINDOW_DIALOG_H_
