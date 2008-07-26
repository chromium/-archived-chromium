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

#ifndef CHROME_BROWSER_VIEWS_INFO_BAR_CONFIRM_VIEW_H__
#define CHROME_BROWSER_VIEWS_INFO_BAR_CONFIRM_VIEW_H__

#include "chrome/browser/views/info_bar_message_view.h"
#include "chrome/views/native_button.h"

// An info bar with a message, two buttons (labeled OK and Cancel by
// default), and a close button. Can be inherited to override the behavior
// of button presses.
class InfoBarConfirmView : public InfoBarMessageView,
                           public ChromeViews::NativeButton::Listener {
 public:
  explicit InfoBarConfirmView(const std::wstring& message);

  virtual ~InfoBarConfirmView();

  // Invoked when the OK button is pressed. Closes info bar by default.
  virtual void OKButtonPressed();

  // Invoked when the Cancel button is pressed. Closes info bar by default.
  virtual void CancelButtonPressed();

  // ButtonListener Method:
  // Invokes OKButtonPressed or CancelButtonPressed() when their
  // respective buttons are pressed.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // Sets the label on the OK button, if it exists.
  void SetOKButtonLabel(const std::wstring& label);

  // Sets the label on the Cancel button, if it exists.
  void SetCancelButtonLabel(const std::wstring& label);

  // Removes the cancel button from the info bar.
  // Can't be re-added.
  void RemoveCancelButton();

  // Removes the OK button from the info bar.
  // Can't be re-added.
  void RemoveOKButton();

  // Returns the MSAA role of the current view. The role is what assistive
  // technologies (ATs) use to determine what behavior to expect from a given
  // control.
  bool GetAccessibleRole(VARIANT* role);

 private:
  // Creates the ok and cancel buttons. And then calls the InfoBarMessageViews
  // init to set up the message and close buttons which will
  // then call AddAllChildViews.
  void Init();

  ChromeViews::NativeButton* ok_button_;

  ChromeViews::NativeButton* cancel_button_;

  DISALLOW_EVIL_CONSTRUCTORS(InfoBarConfirmView);
};

#endif // CHROME_BROWSER_VIEWS_INFO_BAR_CONFIRM_VIEW_H__