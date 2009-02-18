// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_DIALOG_DELEGATE_H_
#define CHROME_VIEWS_DIALOG_DELEGATE_H_

#include "chrome/views/dialog_client_view.h"
#include "chrome/views/window_delegate.h"

namespace views {

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
  virtual View* GetExtraView() { return NULL; }

  // Returns the default dialog button. This should not be a mask as only one
  // button should ever be the default button. Return DIALOGBUTTON_NONE if
  // there is no default.  Default behavior is to return DIALOGBUTTON_OK or
  // DIALOGBUTTON_CANCEL (in that order) if they are present, DIALOGBUTTON_NONE
  // otherwise.
  virtual int GetDefaultDialogButton() const;

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

  // Overridden from WindowDelegate:
  virtual View* GetInitiallyFocusedView();

  // Overridden from WindowDelegate:
  virtual ClientView* CreateClientView(Window* window);

  // A helper for accessing the DialogClientView object contained by this
  // delegate's Window.
  DialogClientView* GetDialogClientView() const;
};

}  // namespace views

#endif  // CHROME_VIEWS_DIALOG_DELEGATE_H_
