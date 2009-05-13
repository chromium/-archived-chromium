// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/logging.h"
#include "chrome/browser/views/user_data_dir_dialog.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

// static
std::wstring UserDataDirDialog::RunUserDataDirDialog(
    const std::wstring& user_data_dir) {
  // When the window closes, it will delete itself.
  UserDataDirDialog* dlg = new UserDataDirDialog(user_data_dir);
  MessageLoopForUI::current()->Run(dlg);
  return dlg->user_data_dir();
}

UserDataDirDialog::UserDataDirDialog(const std::wstring& user_data_dir)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          select_file_dialog_(SelectFileDialog::Create(this))),
      is_blocking_(true) {
  std::wstring message_text = l10n_util::GetStringF(
      IDS_CANT_WRITE_USER_DIRECTORY_SUMMARY, user_data_dir);
  const int kDialogWidth = 400;
  message_box_view_ = new MessageBoxView(MessageBoxFlags::kIsConfirmMessageBox,
      message_text.c_str(), std::wstring(), kDialogWidth);

  views::Window::CreateChromeWindow(NULL, gfx::Rect(), this)->Show();
}

UserDataDirDialog::~UserDataDirDialog() {
  select_file_dialog_->ListenerDestroyed();
}

std::wstring UserDataDirDialog::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {

  switch (button) {
    case MessageBoxFlags::DIALOGBUTTON_OK:
      return l10n_util::GetString(
          IDS_CANT_WRITE_USER_DIRECTORY_CHOOSE_DIRECTORY_BUTTON);
    case MessageBoxFlags::DIALOGBUTTON_CANCEL:
      return l10n_util::GetString(IDS_CANT_WRITE_USER_DIRECTORY_EXIT_BUTTON);
    default:
      NOTREACHED();
  }

  return std::wstring();
}

std::wstring UserDataDirDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_CANT_WRITE_USER_DIRECTORY_TITLE);
}

void UserDataDirDialog::DeleteDelegate() {
  delete this;
}

bool UserDataDirDialog::Accept() {
  // Directory picker
  std::wstring dialog_title = l10n_util::GetString(
      IDS_CANT_WRITE_USER_DIRECTORY_CHOOSE_DIRECTORY_BUTTON);
  HWND owning_hwnd =
      GetAncestor(message_box_view_->GetWidget()->GetNativeView(), GA_ROOT);
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_FOLDER,
                                  dialog_title, FilePath(), NULL,
                                  0, std::wstring(), owning_hwnd, NULL);
  return false;
}

bool UserDataDirDialog::Cancel() {
  is_blocking_ = false;
  return true;
}

views::View* UserDataDirDialog::GetContentsView() {
  return message_box_view_;
}

bool UserDataDirDialog::Dispatch(const MSG& msg) {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return is_blocking_;
}

void UserDataDirDialog::FileSelected(const FilePath& path,
                                     int index, void* params) {
  user_data_dir_ = path.ToWStringHack();
  is_blocking_ = false;
}

void UserDataDirDialog::FileSelectionCanceled(void* params) {
}
