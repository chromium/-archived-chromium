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

#ifndef CHROME_VIEWS_DIALOG_DELEGATE_H__
#define CHROME_VIEWS_DIALOG_DELEGATE_H__

#include "chrome/views/window_delegate.h"

namespace ChromeViews {

class NativeButton;
class View;

///////////////////////////////////////////////////////////////////////////////
//
// DialogDelegate
//
//  DialogDelegate is an interface implemented by objects that wish to show a
//  dialog box Window. The window that is displayed uses this interface to
//  determine how it should be displayed and notify the delegate object of
//  certain events.
//
///////////////////////////////////////////////////////////////////////////////
class DialogDelegate : public WindowDelegate {
 public:
  virtual DialogDelegate* AsDialogDelegate() { return this; }

  enum DialogButton {
    DIALOGBUTTON_NONE = 0,    // No dialog buttons, for WindowType == WINDOW.
    DIALOGBUTTON_OK = 1,      // Has an OK button.
    DIALOGBUTTON_CANCEL = 2,  // Has a Cancel button (becomes a Close button if
  };                          // no OK button).

  // Returns a mask specifying which of the available DialogButtons are visible
  // for the dialog. Note: If an OK button is provided, you should provide a
  // CANCEL button. A dialog box with just an OK button is frowned upon and
  // considered a very special case, so if you're planning on including one,
  // you should reconsider, or beng says there will be stabbings.
  //
  // To use the extra button you need to override GetDialogButtons()
  virtual int GetDialogButtons() const {
    return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;
  }

  // Returns whether accelerators are enabled on the button. This is invoked
  // when an accelerator is pressed, not at construction time. This
  // returns true.
  virtual bool AreAcceleratorsEnabled(DialogButton button) { return true; }

  // Returns the label of the specified DialogButton.
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const {
    // empty string results in defaults for DIALOGBUTTON_OK,
    // DIALOGBUTTON_CANCEL.
    return L"";
  }

  // Override this function if with a view which will be shown in the same
  // row as the OK and CANCEL buttons but flush to the left and extending
  // up to the buttons.
  virtual ChromeViews::View* GetExtraView() { return NULL; }

  // Returns the default dialog button. This should not be a mask as only one
  // button should ever be the default button. Return DIALOGBUTTON_NONE if
  // there is no default.
  virtual int GetDefaultDialogButton() const {
    return DIALOGBUTTON_OK;
  }

  // Returns whether the specified dialog button is enabled.
  virtual bool IsDialogButtonEnabled(DialogButton button) const {
    return true;
  }

  // Returns whether the specified dialog button is visible.
  virtual bool IsDialogButtonVisible(DialogButton button) const {
    return true;
  }

  // For Dialog boxes, if there is a "Cancel" button, this is called when the
  // user presses the "Cancel" button or the Close button on the window or
  // in the system menu, or presses the Esc key. This function should return
  // true if the window can be closed after it returns, or false if it must
  // remain open.
  virtual bool Cancel() { return true; }

  // For Dialog boxes, this is called when the user presses the "OK" button,
  // or the Enter key. Can also be called on Esc key or close button presses if
  // there is no "Cancel" button. This function should return true if the window
  // can be closed after the window can be closed after it returns, or false if
  // it must remain open. If |window_closing| is true, it means that this
  // handler is being called because the window is being closed (e.g. by
  // Window::Close) and there is no Cancel handler, so Accept is being called
  // instead.
  virtual bool Accept(bool window_closing) { return Accept(); }
  virtual bool Accept() { return true; }
};

}  // namespace ChromeViews

#endif  // #ifndef CHROME_VIEWS_DIALOG_DELEGATE_H__
