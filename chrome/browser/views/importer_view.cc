// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/importer_view.h"

#include "app/l10n_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using views::ColumnSet;
using views::GridLayout;

namespace browser {

// Declared in browser_dialogs.h so caller's don't have to depend on our header.
void ShowImporterView(views::Widget* parent,
                      Profile* profile) {
  views::Window::CreateChromeWindow(parent->GetNativeView(), gfx::Rect(),
                                    new ImporterView(profile))->Show();
}

}  // namespace browser

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

  profile_combobox_ = new views::Combobox(this);
  profile_combobox_->set_listener(this);

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
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
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
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    return false;
  }

  uint16 items = GetCheckedItems();

  int selected_index = profile_combobox_->selected_item();
  StartImportingWithUI(GetWidget()->GetNativeView(), items,
                       importer_host_.get(),
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

int ImporterView::GetItemCount(views::Combobox* source) {
  DCHECK(source == profile_combobox_);
  DCHECK(importer_host_.get());
  int item_count = importer_host_->GetAvailableProfileCount();
  if (checkbox_items_.size() < static_cast<size_t>(item_count))
    checkbox_items_.resize(item_count, ALL);
  return item_count;
}

std::wstring ImporterView::GetItemAt(views::Combobox* source, int index) {
  DCHECK(source == profile_combobox_);
  DCHECK(importer_host_.get());
  return importer_host_->GetSourceProfileNameAt(index);
}

void ImporterView::ItemChanged(views::Combobox* combobox,
                               int prev_index, int new_index) {
  DCHECK(combobox);
  DCHECK(checkbox_items_.size() >=
      static_cast<size_t>(importer_host_->GetAvailableProfileCount()));

  if (prev_index == new_index)
    return;

  // Save the current state
  uint16 prev_items = GetCheckedItems();
  checkbox_items_[prev_index] = prev_items;

  // Enable/Disable the checkboxes for this Item
  uint16 new_enabled_items = importer_host_->GetSourceProfileInfoAt(
      new_index).services_supported;
  SetCheckedItemsState(new_enabled_items);

  // Set the checked items for this Item
  uint16 new_items = checkbox_items_[new_index];
  SetCheckedItems(new_items);
}

void ImporterView::ImportCanceled() {
  ImportComplete();
}

void ImporterView::ImportComplete() {
  // Now close this window since the import completed or was canceled.
  window()->Close();
}

views::Checkbox* ImporterView::InitCheckbox(const std::wstring& text,
                                            bool checked) {
  views::Checkbox* checkbox = new views::Checkbox(text);
  checkbox->SetChecked(checked);
  return checkbox;
}

uint16 ImporterView::GetCheckedItems() {
  uint16 items = NONE;
  if (history_checkbox_->IsEnabled() && history_checkbox_->checked())
    items |= HISTORY;
  if (favorites_checkbox_->IsEnabled() && favorites_checkbox_->checked())
    items |= FAVORITES;
  if (passwords_checkbox_->IsEnabled() && passwords_checkbox_->checked())
    items |= PASSWORDS;
  if (search_engines_checkbox_->IsEnabled() &&
      search_engines_checkbox_->checked())
    items |= SEARCH_ENGINES;
  return items;
}

void ImporterView::SetCheckedItemsState(uint16 items) {
  if (items & HISTORY) {
    history_checkbox_->SetEnabled(true);
  } else {
    history_checkbox_->SetEnabled(false);
    history_checkbox_->SetChecked(false);
  }
  if (items & FAVORITES) {
    favorites_checkbox_->SetEnabled(true);
  } else {
    favorites_checkbox_->SetEnabled(false);
    favorites_checkbox_->SetChecked(false);
  }
  if (items & PASSWORDS) {
    passwords_checkbox_->SetEnabled(true);
  } else {
    passwords_checkbox_->SetEnabled(false);
    passwords_checkbox_->SetChecked(false);
  }
  if (items & SEARCH_ENGINES) {
    search_engines_checkbox_->SetEnabled(true);
  } else {
    search_engines_checkbox_->SetEnabled(false);
    search_engines_checkbox_->SetChecked(false);
  }
}

void ImporterView::SetCheckedItems(uint16 items) {
  if (history_checkbox_->IsEnabled())
    history_checkbox_->SetChecked(!!(items & HISTORY));

  if (favorites_checkbox_->IsEnabled())
    favorites_checkbox_->SetChecked(!!(items & FAVORITES));

  if (passwords_checkbox_->IsEnabled())
    passwords_checkbox_->SetChecked(!!(items & PASSWORDS));

  if (search_engines_checkbox_->IsEnabled())
    search_engines_checkbox_->SetChecked(!!(items & SEARCH_ENGINES));
}
