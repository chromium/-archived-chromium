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

#ifndef CHROME_BROWSER_VIEWS_RESTART_MESSAGE_BOX_H_
#define CHROME_BROWSER_VIEWS_RESTART_MESSAGE_BOX_H_

#include "base/basictypes.h"
#include "chrome/views/dialog_delegate.h"

class MessageBoxView;

// A dialog box that tells the user that s/he needs to restart Chrome
// for a change to take effect.
class RestartMessageBox : public ChromeViews::DialogDelegate {
 public:
  // This box is modal to |parent_hwnd|.
  static void ShowMessageBox(HWND parent_hwnd);

 protected:
  // ChromeViews::DialogDelegate:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;

  // ChromeViews::WindowDelegate:
  virtual void WindowClosing();
  virtual bool IsModal() const;
  virtual ChromeViews::View* GetContentsView();

 private:
  explicit RestartMessageBox(HWND parent_hwnd);
  virtual ~RestartMessageBox();

  MessageBoxView* message_box_view_;

  DISALLOW_EVIL_CONSTRUCTORS(RestartMessageBox);
};

#endif  // CHROME_BROWSER_VIEWS_RESTART_MESSAGE_BOX_H_
