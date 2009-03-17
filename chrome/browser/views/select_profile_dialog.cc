// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/select_profile_dialog.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/user_data_manager.h"
#include "chrome/browser/views/new_profile_dialog.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/combo_box.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/message_box_view.h"
#include "chrome/views/view.h"
#include "chrome/views/window/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

using views::ColumnSet;
using views::GridLayout;

// static
void SelectProfileDialog::RunDialog() {
  // When the window closes, it will delete itself.
  SelectProfileDialog* dlg = new SelectProfileDialog();
  views::Window::CreateChromeWindow(NULL, gfx::Rect(), dlg)->Show();
}

SelectProfileDialog::SelectProfileDialog()
    : helper_(new GetProfilesHelper(this)) {
  // We first create an instance of the helper and then setup controls. This
  // doesn't lead to race condition because once the helper is done with
  // enumerating profiles by examining the file system, it posts a task on the
  // thread it was called on. This is the same thread that the current code is
  // running on. So that task wouldn't get executed until we are done with
  // setup controls. Given that, we start the helper before setup controls so
  // that file enumeration can be done as soon as possible.
  helper_->GetProfiles(NULL);
  SetupControls();
}

SelectProfileDialog::~SelectProfileDialog() {
  helper_->OnDelegateDeleted();
}

gfx::Size SelectProfileDialog::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_SELECT_PROFILE_DIALOG_WIDTH_CHARS,
      IDS_SELECT_PROFILE_DIALOG_HEIGHT_LINES));
}

void SelectProfileDialog::PopulateProfilesComboBox(
    const std::vector<std::wstring>& profiles) {
  profiles_.insert(profiles_.begin(), profiles.begin(), profiles.end());
  profile_combobox_->ModelChanged();
  GetDialogClientView()->UpdateDialogButtons();
}

void SelectProfileDialog::Layout() {
  GetLayoutManager()->Layout(this);
}

int SelectProfileDialog::GetDialogButtons() const {
  return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;
}

views::View* SelectProfileDialog::GetInitiallyFocusedView() {
  return profile_combobox_;
}

std::wstring SelectProfileDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_SELECT_PROFILE_DIALOG_TITLE);
}

bool SelectProfileDialog::Accept() {
  int index = profile_combobox_->GetSelectedItem();
  if (index < 0) {
    NOTREACHED();
    return true;
  }

  // If the user has selected <New Profile> from the drop down, then show the
  // new profile dialog to the user.
  if (index == profiles_.size()) {
    NewProfileDialog::RunDialog();
    return true;
  }

  std::wstring profile_name = profiles_[index];
  UserDataManager::Get()->LaunchChromeForProfile(profile_name);
  return true;
}

bool SelectProfileDialog::Cancel() {
  return true;
}

views::View* SelectProfileDialog::GetContentsView() {
  return this;
}

int SelectProfileDialog::GetItemCount(views::ComboBox* source) {
  // Always show one more item in the combo box that allows the user to select
  // <New Profile>.
  return profiles_.size() + 1;
}

std::wstring SelectProfileDialog::GetItemAt(views::ComboBox* source,
                                            int index) {
  DCHECK(source == profile_combobox_);
  DCHECK(index >= 0 && index <= static_cast<int>(profiles_.size()));
  // For the last item in the drop down, return the <New Profile> text,
  // otherwise return the corresponding profile name from the vector.
  return index == profiles_.size() ?
      l10n_util::GetString(IDS_SELECT_PROFILE_DIALOG_NEW_PROFILE_ENTRY) :
      profiles_[index];
}

void SelectProfileDialog::OnGetProfilesDone(
    const std::vector<std::wstring>& profiles) {
  PopulateProfilesComboBox(profiles);
}

void SelectProfileDialog::SetupControls() {
  // Adds all controls.
  select_profile_label_ = new views::Label(
      l10n_util::GetString(IDS_SELECT_PROFILE_DIALOG_LABEL_TEXT));
  profile_combobox_ = new views::ComboBox(this);

  // Arranges controls by using GridLayout.
  const int column_set_id = 0;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::FIXED, 200, 0);

  layout->StartRow(0, column_set_id);
  layout->AddView(select_profile_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(profile_combobox_);
}
