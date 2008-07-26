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

#ifndef CHROME_BROWSER_VIEWS_INPUT_WINDOW_H__
#define CHROME_BROWSER_VIEWS_INPUT_WINDOW_H__

#include "chrome/views/dialog_delegate.h"

// InputWindowDelegate --------------------------------------------------------

class InputWindowDelegate : public ChromeViews::DialogDelegate {
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


namespace ChromeViews {
class Window;
};

ChromeViews::Window* CreateInputWindow(HWND parent_hwnd,
                                       InputWindowDelegate* delegate);

#endif  // CHROME_BROWSER_VIEWS_INPUT_WINDOW_H__
