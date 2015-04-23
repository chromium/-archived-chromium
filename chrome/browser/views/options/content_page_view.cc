// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/content_page_view.h"

#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/native_theme.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/views/clear_browsing_data.h"
#include "chrome/browser/views/importer_view.h"
#include "chrome/browser/views/options/options_group_view.h"
#include "chrome/browser/views/options/passwords_exceptions_window_view.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "views/controls/button/radio_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"

namespace {

const int kPasswordSavingRadioGroup = 1;
const int kFormAutofillRadioGroup = 2;
}  // namespace

ContentPageView::ContentPageView(Profile* profile)
    : passwords_exceptions_button_(NULL),
      passwords_group_(NULL),
      passwords_asktosave_radio_(NULL),
      passwords_neversave_radio_(NULL),
      form_autofill_asktosave_radio_(NULL),
      form_autofill_neversave_radio_(NULL),
      themes_group_(NULL),
      themes_reset_button_(NULL),
      browsing_data_label_(NULL),
      browsing_data_group_(NULL),
      import_button_(NULL),
      clear_data_button_(NULL),
      OptionsPageView(profile) {
}

ContentPageView::~ContentPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageView, views::ButtonListener implementation:

void ContentPageView::ButtonPressed(views::Button* sender) {
  if (sender == passwords_asktosave_radio_ ||
      sender == passwords_neversave_radio_) {
    bool enabled = passwords_asktosave_radio_->checked();
    if (enabled) {
      UserMetricsRecordAction(L"Options_PasswordManager_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_PasswordManager_Disable",
                              profile()->GetPrefs());
    }
    ask_to_save_passwords_.SetValue(enabled);
  } else if (sender == form_autofill_asktosave_radio_ ||
             sender == form_autofill_neversave_radio_) {
    bool enabled = form_autofill_asktosave_radio_->checked();
    if (enabled) {
      UserMetricsRecordAction(L"Options_FormAutofill_Enable",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_FormAutofill_Disable",
                              profile()->GetPrefs());
    }
    ask_to_save_form_autofill_.SetValue(enabled);
  } else if (sender == passwords_exceptions_button_) {
    UserMetricsRecordAction(L"Options_ShowPasswordsExceptions", NULL);
    PasswordsExceptionsWindowView::Show(profile());
  } else if (sender == themes_reset_button_) {
    UserMetricsRecordAction(L"Options_ThemesReset", profile()->GetPrefs());
    profile()->ClearTheme();
  } else if (sender == import_button_) {
    views::Window::CreateChromeWindow(
      GetWindow()->GetNativeWindow(),
      gfx::Rect(),
      new ImporterView(profile()))->Show();
  } else if (sender == clear_data_button_) {
    views::Window::CreateChromeWindow(
      GetWindow()->GetNativeWindow(),
      gfx::Rect(),
      new ClearBrowsingDataView(profile()))->Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ContentPageView, OptionsPageView implementation:

void ContentPageView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, single_column_view_set_id);
  InitPasswordSavingGroup();
  layout->AddView(passwords_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitFormAutofillGroup();
  layout->AddView(form_autofill_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitBrowsingDataGroup();
  layout->AddView(browsing_data_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitThemesGroup();
  layout->AddView(themes_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Init member prefs so we can update the controls if prefs change.
  ask_to_save_passwords_.Init(prefs::kPasswordManagerEnabled,
                              profile()->GetPrefs(), this);
  ask_to_save_form_autofill_.Init(prefs::kFormAutofillEnabled,
                                  profile()->GetPrefs(), this);
}

void ContentPageView::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kPasswordManagerEnabled) {
    if (ask_to_save_passwords_.GetValue()) {
      passwords_asktosave_radio_->SetChecked(true);
    } else {
      passwords_neversave_radio_->SetChecked(true);
    }
  }
  if (!pref_name || *pref_name == prefs::kFormAutofillEnabled) {
    if (ask_to_save_form_autofill_.GetValue()) {
      form_autofill_asktosave_radio_->SetChecked(true);
    } else {
      form_autofill_neversave_radio_->SetChecked(true);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// ContentsPageView, views::View overrides:

void ContentPageView::Layout() {
  // We need to Layout twice - once to get the width of the contents box...
  View::Layout();
  passwords_asktosave_radio_->SetBounds(
      0, 0, passwords_group_->GetContentsWidth(), 0);
  passwords_neversave_radio_->SetBounds(
      0, 0, passwords_group_->GetContentsWidth(), 0);
  browsing_data_label_->SetBounds(
      0, 0, browsing_data_group_->GetContentsWidth(), 0);
  // ... and twice to get the height of multi-line items correct.
  View::Layout();
}

///////////////////////////////////////////////////////////////////////////////
// ContentPageView, private:

void ContentPageView::InitPasswordSavingGroup() {
  passwords_asktosave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_ASKTOSAVE),
      kPasswordSavingRadioGroup);
  passwords_asktosave_radio_->set_listener(this);
  passwords_asktosave_radio_->SetMultiLine(true);
  passwords_neversave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_NEVERSAVE),
      kPasswordSavingRadioGroup);
  passwords_neversave_radio_->set_listener(this);
  passwords_neversave_radio_->SetMultiLine(true);
  passwords_exceptions_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_OPTIONS_PASSWORDS_EXCEPTIONS));

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_asktosave_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_neversave_radio_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(passwords_exceptions_button_);

  passwords_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_PASSWORDS_GROUP_NAME), L"",
      true);
}

void ContentPageView::InitFormAutofillGroup() {
  form_autofill_asktosave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_AUTOFILL_SAVE),
      kFormAutofillRadioGroup);
  form_autofill_asktosave_radio_->set_listener(this);
  form_autofill_asktosave_radio_->SetMultiLine(true);
  form_autofill_neversave_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_AUTOFILL_NEVERSAVE),
      kFormAutofillRadioGroup);
  form_autofill_neversave_radio_->set_listener(this);
  form_autofill_neversave_radio_->SetMultiLine(true);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(form_autofill_asktosave_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(form_autofill_neversave_radio_);

  form_autofill_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME),
      L"", true);
}

void ContentPageView::InitThemesGroup() {
  themes_reset_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_THEMES_RESET_BUTTON));

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
      GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(themes_reset_button_);

  themes_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_THEMES_GROUP_NAME),
      L"", false);
}

void ContentPageView::InitBrowsingDataGroup() {
  clear_data_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_OPTIONS_CLEAR_DATA_BUTTON));
  import_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_OPTIONS_IMPORT_DATA_BUTTON));
  browsing_data_label_ = new views::Label(
      l10n_util::GetString(IDS_OPTIONS_BROWSING_DATA_INFO));
  browsing_data_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  browsing_data_label_->SetMultiLine(true);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  // Add the browsing data label component.
  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
      GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(browsing_data_label_);

  // Add some padding for not making the next component close together.
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add double column layout for import and clear browsing buttons.
  const int double_column_view_set_id = 1;
  ColumnSet* double_col_set = layout->AddColumnSet(double_column_view_set_id);
  double_col_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
  double_col_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  double_col_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(import_button_);
  layout->AddView(clear_data_button_);

  browsing_data_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME),
      L"", true);
}
