// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INPUT_WINDOW_H__
#define CHROME_BROWSER_VIEWS_INPUT_WINDOW_H__

#include "chrome/views/dialog_delegate.h"

// InputWindowDelegate --------------------------------------------------------

class InputWindowDelegate : public views::DialogDelegate {
 public:
  // Returns the text displayed on the label preceding the text field.
  virtual std::wstring GetTextFieldLabel() = 0;

  // Returns the initial contents of the text field.
  virtual std::wstring GetTextFieldContents() {
    return std::wstring();
  }

  // Returns whether the text is valid. InputAccepted is only invoked if the
  // text is valid.
  virtual bool IsValid(const std::wstring& text) {
    return true;
  }

  // Invoked when the user presses the ok button and the text is fvalid.
  virtual void InputAccepted(const std::wstring& text) = 0;

  // Invoked when the user cancels the dialog.
  virtual void InputCanceled() {}
};


namespace views {
class Window;
};

views::Window* CreateInputWindow(HWND parent_hwnd,
                                 InputWindowDelegate* delegate);

#endif  // CHROME_BROWSER_VIEWS_INPUT_WINDOW_H__
