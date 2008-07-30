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
//
// A dialog box that tells the user that we can't write to the specified user
// data directory.  Provides the user a chance to pick a different directory.

#ifndef CHROME_BROWSER_USER_DATA_DIR_DIALOG_H__
#define CHROME_BROWSER_USER_DATA_DIR_DIALOG_H__

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/views/dialog_delegate.h"

class MessageBoxView;
namespace ChromeViews {
class Window;
}

class UserDataDirDialog : public ChromeViews::DialogDelegate,
                          public MessageLoop::Dispatcher,
                          public SelectFileDialog::Listener {
 public:
  // Creates and runs a user data directory picker dialog.  The method blocks
  // while the dialog is showing.  If the user picks a directory, this method
  // returns the chosen directory. |user_data_dir| is the value of the
  // directory we were not able to use.
  static std::wstring RunUserDataDirDialog(const std::wstring& user_data_dir);
  virtual ~UserDataDirDialog();

  std::wstring user_data_dir() { return user_data_dir_; }

  // ChromeViews::DialogDelegate Methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Accept();
  virtual bool Cancel();

  // ChromeViews::WindowDelegate Methods:
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool IsModal() const { return false; }
  virtual ChromeViews::View* GetContentsView();

  // MessageLoop::Dispatcher Method:
  virtual bool Dispatch(const MSG& msg);

  // SelectFileDialog::Listener Methods:
  virtual void FileSelected(const std::wstring& path, void* params);
  virtual void FileSelectionCanceled(void* params);

 private:
  explicit UserDataDirDialog(const std::wstring& user_data_dir);

  // Empty until the user picks a directory.
  std::wstring user_data_dir_;

  MessageBoxView* message_box_view_;
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Used to keep track of whether or not to block the message loop (still
  // waiting for the user to dismiss the dialog).
  bool is_blocking_;

  DISALLOW_EVIL_CONSTRUCTORS(UserDataDirDialog);
};

#endif // CHROME_BROWSER_USER_DATA_DIR_DIALOG_H__
