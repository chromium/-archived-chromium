// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/new_profile_dialog.h"

#include <string>

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/logging.h"
#include "base/file_util.h"
#include "chrome/browser/user_data_manager.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/message_box_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/window/window.h"

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowNewProfileDialog() {
  NewProfileDialog::RunDialog();
}

}  // namespace browser

// static
void NewProfileDialog::RunDialog() {
  NewProfileDialog* dlg = new NewProfileDialog();
  views::Window::CreateChromeWindow(NULL, gfx::Rect(), dlg)->Show();
}

NewProfileDialog::NewProfileDialog() {
  std::wstring message_text = l10n_util::GetString(
      IDS_NEW_PROFILE_DIALOG_LABEL_TEXT);
  const int kDialogWidth = views::Window::GetLocalizedContentsWidth(
      IDS_NEW_PROFILE_DIALOG_WIDTH_CHARS);
  const int kMessageBoxFlags = MessageBoxFlags::kFlagHasOKButton |
                               MessageBoxFlags::kFlagHasCancelButton |
                               MessageBoxFlags::kFlagHasPromptField;
  message_box_view_ = new MessageBoxView(kMessageBoxFlags,
                                         message_text.c_str(),
                                         std::wstring(),
                                         kDialogWidth);
  message_box_view_->SetCheckBoxLabel(
      l10n_util::GetString(IDS_NEW_PROFILE_DIALOG_CREATE_SHORTCUT_TEXT));
  message_box_view_->SetCheckBoxSelected(true);
  message_box_view_->text_box()->SetController(this);
}

NewProfileDialog::~NewProfileDialog() {
}

views::View* NewProfileDialog::GetInitiallyFocusedView() {
  views::Textfield* text_box = message_box_view_->text_box();
  DCHECK(text_box);
  return text_box;
}

bool NewProfileDialog::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    std::wstring profile_name = message_box_view_->GetInputText();
    // TODO(munjal): Refactor the function ReplaceIllegalCharacters in
    // file_util to something that just checks if there are illegal chars
    // since that's what we really need. Also, replaceIllegalChars seems to
    // be expensive since it builds a list of illegal characters for each call.
    // So at the least fix that.
    file_util::ReplaceIllegalCharacters(&profile_name, L'_');
    return !profile_name.empty() &&
        profile_name == message_box_view_->GetInputText();
  }
  return true;
}

std::wstring NewProfileDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_NEW_PROFILE_DIALOG_TITLE);
}

void NewProfileDialog::DeleteDelegate() {
  delete this;
}

void NewProfileDialog::ContentsChanged(views::Textfield* sender,
                                       const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
}

bool NewProfileDialog::Accept() {
  std::wstring profile_name = message_box_view_->GetInputText();
  if (profile_name.empty()) {
    NOTREACHED();
    return true;
  }
  // Create a desktop shortcut if the corresponding checkbox is checked.
  if (message_box_view_->IsCheckBoxSelected())
    UserDataManager::Get()->CreateDesktopShortcutForProfile(
        profile_name);

  UserDataManager::Get()->LaunchChromeForProfile(profile_name);
  return true;
}

views::View* NewProfileDialog::GetContentsView() {
  return message_box_view_;
}
