// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/importer_view.h"

#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/window.h"

#include "generated_resources.h"

using views::ColumnSet;
using views::GridLayout;

ImporterView::ImporterView(Profile* profile)
    : import_from_label_(NULL),
      profile_combobox_(NULL),
      import_items_label_(NULL),
      history_checkbox_(NULL),
      favorites_checkbox_(NULL),
      passwords_checkbox_(NULL),
      search_engines_checkbox_(NULL),
      profile_(profile),
      importer_host_(new ImporterHost()) {
  DCHECK(profile);
  SetupControl();
}

ImporterView::~ImporterView() {
}

void ImporterView::SetupControl() {
  // Adds all controls.
  import_from_label_ =
      new views::Label(l10n_util::GetString(IDS_IMPORT_FROM_LABEL));

  profile_combobox_ = new views::ComboBox(this);

  import_items_label_ =
      new views::Label(l10n_util::GetString(IDS_IMPORT_ITEMS_LABEL));

  history_checkbox_ =
      InitCheckbox(l10n_util::GetString(IDS_IMPORT_HISTORY_CHKBOX), true);
  favorites_checkbox_ =
      InitCheckbox(l10n_util::GetString(IDS_IMPORT_FAVORITES_CHKBOX), true);
  passwords_checkbox_ =
      InitCheckbox(l10n_util::GetString(IDS_IMPORT_PASSWORDS_CHKBOX), true);
  search_engines_checkbox_ =
      InitCheckbox(l10n_util::GetString(IDS_IMPORT_SEARCH_ENGINES_CHKBOX),
                   true);

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
  layout->AddView(import_from_label_);
  layout->AddView(profile_combobox_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(import_items_label_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(favorites_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(search_engines_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(passwords_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(history_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

gfx::Size ImporterView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_IMPORT_DIALOG_WIDTH_CHARS,
      IDS_IMPORT_DIALOG_HEIGHT_LINES));
}

void ImporterView::Layout() {
  GetLayoutManager()->Layout(this);
}

std::wstring ImporterView::GetDialogButtonLabel(
    DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_IMPORT_COMMIT);
  } else {
    return std::wstring();
  }
}

bool ImporterView::IsModal() const {
  return true;
}

std::wstring ImporterView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_IMPORT_SETTINGS_TITLE);
}

bool ImporterView::Accept() {
  if (!IsDialogButtonEnabled(DIALOGBUTTON_OK)) {
    return false;
  }

  uint16 items = NONE;
  if (history_checkbox_->IsEnabled() && history_checkbox_->IsSelected())
    items |= HISTORY;
  if (favorites_checkbox_->IsEnabled() && favorites_checkbox_->IsSelected())
    items |= FAVORITES;
  if (passwords_checkbox_->IsEnabled() && passwords_checkbox_->IsSelected())
    items |= PASSWORDS;
  if (search_engines_checkbox_->IsEnabled() &&
      search_engines_checkbox_->IsSelected())
    items |= SEARCH_ENGINES;

  Browser* browser = BrowserList::GetLastActive();
  int selected_index = profile_combobox_->GetSelectedItem();
  StartImportingWithUI(browser->GetTopLevelHWND(), items, importer_host_.get(),
                       importer_host_->GetSourceProfileInfoAt(selected_index),
                       profile_, this, false);
  // We return false here to prevent the window from being closed. We will be
  // notified back by our implementation of ImportObserver when the import is
  // complete so that we can close ourselves.
  return false;
}

views::View* ImporterView::GetContentsView() {
  return this;
}

int ImporterView::GetItemCount(views::ComboBox* source) {
  DCHECK(source == profile_combobox_);
  DCHECK(importer_host_.get());
  return importer_host_->GetAvailableProfileCount();
}

std::wstring ImporterView::GetItemAt(views::ComboBox* source, int index) {
  DCHECK(source == profile_combobox_);
  DCHECK(importer_host_.get());
  return importer_host_->GetSourceProfileNameAt(index);
}

void ImporterView::ImportCanceled() {
  ImportComplete();
}

void ImporterView::ImportComplete() {
  // Now close this window since the import completed or was canceled.
  window()->Close();
}

views::CheckBox* ImporterView::InitCheckbox(const std::wstring& text,
                                            bool checked) {
  views::CheckBox* checkbox = new views::CheckBox(text);
  checkbox->SetIsSelected(checked);
  return checkbox;
}

