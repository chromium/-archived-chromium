// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/uninstall_dialog.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/message_loop.h"
#include "chrome/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

// static
void UninstallDialog::ShowUninstallDialog(int& user_selection) {
  // When the window closes, it will delete itself.
  new UninstallDialog(user_selection);
}

bool UninstallDialog::Accept() {
  user_selection_ = ResultCodes::NORMAL_EXIT;
  if (message_box_view_->IsCheckBoxSelected())
    user_selection_ = ResultCodes::UNINSTALL_DELETE_PROFILE;
  return true;
}

bool UninstallDialog::Cancel() {
  user_selection_ = ResultCodes::UNINSTALL_USER_CANCEL;
  return true;
}

std::wstring UninstallDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_UNINSTALL_CHROME);
}

void UninstallDialog::DeleteDelegate() {
  delete this;
}

views::View* UninstallDialog::GetContentsView() {
  return message_box_view_;
}

UninstallDialog::UninstallDialog(int& user_selection)
    : user_selection_(user_selection) {
  // Also deleted when the window closes.
  message_box_view_ = new MessageBoxView(
      MessageBoxFlags::kIsConfirmMessageBox |
          MessageBoxFlags::kAutoDetectAlignment,
      l10n_util::GetString(IDS_UNINSTALL_VERIFY).c_str(),
      std::wstring());
  message_box_view_->SetCheckBoxLabel(
      l10n_util::GetString(IDS_UNINSTALL_DELETE_PROFILE));
  message_box_view_->SetCheckBoxSelected(false);
  views::Window::CreateChromeWindow(NULL, gfx::Rect(), this)->Show();
}

UninstallDialog::~UninstallDialog() {
  MessageLoop::current()->Quit();
}
