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

#include "base/logging.h"
#include "chrome/browser/user_data_dir_dialog.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/message_box_view.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

// static
std::wstring UserDataDirDialog::RunUserDataDirDialog(
    const std::wstring& user_data_dir) {
  // When the window closes, it will delete itself.
  UserDataDirDialog* dlg = new UserDataDirDialog(user_data_dir);
  MessageLoop::current()->Run(dlg);
  return dlg->user_data_dir();
}

UserDataDirDialog::UserDataDirDialog(const std::wstring& user_data_dir)
    : select_file_dialog_(SelectFileDialog::Create(this)),
      is_blocking_(true) {
  std::wstring message_text = l10n_util::GetStringF(
      IDS_CANT_WRITE_USER_DIRECTORY_SUMMARY, user_data_dir);
  const int kDialogWidth = 400;
  message_box_view_ = new MessageBoxView(MessageBoxView::kIsConfirmMessageBox,
      message_text.c_str(), std::wstring(), kDialogWidth);

  ChromeViews::Window::CreateChromeWindow(NULL, gfx::Rect(), this)->Show();
}

UserDataDirDialog::~UserDataDirDialog() {
  select_file_dialog_->ListenerDestroyed();
}

int UserDataDirDialog::GetDialogButtons() const {
  return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;

}

std::wstring UserDataDirDialog::GetDialogButtonLabel(
    DialogButton button) const {

  switch (button) {
    case DIALOGBUTTON_OK:
      return l10n_util::GetString(
          IDS_CANT_WRITE_USER_DIRECTORY_CHOOSE_DIRECTORY_BUTTON);
    case DIALOGBUTTON_CANCEL:
      return l10n_util::GetString(IDS_CANT_WRITE_USER_DIRECTORY_EXIT_BUTTON);
    default:
      NOTREACHED();
  }

  return std::wstring();
}

std::wstring UserDataDirDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_CANT_WRITE_USER_DIRECTORY_TITLE);
}

void UserDataDirDialog::WindowClosing() {
  delete this;
}

bool UserDataDirDialog::Accept() {
  // Directory picker
  std::wstring dialog_title = l10n_util::GetString(
      IDS_CANT_WRITE_USER_DIRECTORY_CHOOSE_DIRECTORY_BUTTON);
  HWND owning_hwnd =
      GetAncestor(message_box_view_->GetViewContainer()->GetHWND(), GA_ROOT);
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_FOLDER,
                                  dialog_title, std::wstring(), owning_hwnd,
                                  NULL);
  return false;
}

bool UserDataDirDialog::Cancel() {
  is_blocking_ = false;
  return true;
}

ChromeViews::View* UserDataDirDialog::GetContentsView() {
  return message_box_view_;
}

bool UserDataDirDialog::Dispatch(const MSG& msg) {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return is_blocking_;
}

void UserDataDirDialog::FileSelected(const std::wstring& path, void* params) {
  user_data_dir_ = path;
  is_blocking_ = false;
  window()->Close();
}

void UserDataDirDialog::FileSelectionCanceled(void* params) {
}
