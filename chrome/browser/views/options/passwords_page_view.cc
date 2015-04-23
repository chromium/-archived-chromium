// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/passwords_page_view.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

using views::ColumnSet;
using views::GridLayout;
using webkit_glue::PasswordForm;

///////////////////////////////////////////////////////////////////////////////
// MultiLabelButtons
MultiLabelButtons::MultiLabelButtons(views::ButtonListener* listener,
                                     const std::wstring& label,
                                     const std::wstring& alt_label)
    : NativeButton(listener, label),
      label_(label),
      alt_label_(alt_label) {
}

gfx::Size MultiLabelButtons::GetPreferredSize() {
  if (pref_size_.IsEmpty()) {
    // Let's compute our preferred size.
    std::wstring current_label = label();
    SetLabel(label_);
    pref_size_ = NativeButton::GetPreferredSize();
    SetLabel(alt_label_);
    gfx::Size alt_pref_size = NativeButton::GetPreferredSize();
    // Revert to the original label.
    SetLabel(current_label);
    pref_size_.SetSize(std::max(pref_size_.width(), alt_pref_size.width()),
                       std::max(pref_size_.height(), alt_pref_size.height()));
  }
  return gfx::Size(pref_size_.width(), pref_size_.height());
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsTableModel, public
PasswordsTableModel::PasswordsTableModel(Profile* profile)
    : observer_(NULL),
      row_count_observer_(NULL),
      pending_login_query_(NULL),
      saved_signons_cleanup_(&saved_signons_),
      profile_(profile) {
  DCHECK(profile && profile->GetWebDataService(Profile::EXPLICIT_ACCESS));
}

PasswordsTableModel::~PasswordsTableModel() {
  CancelLoginsQuery();
}

int PasswordsTableModel::RowCount() {
  return static_cast<int>(saved_signons_.size());
}

std::wstring PasswordsTableModel::GetText(int row,
                                          int col_id) {
  switch (col_id) {
    case IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN: {  // Site.
      const std::wstring& url = saved_signons_[row]->display_url.display_url();
      // Force URL to have LTR directionality.
      if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
        std::wstring localized_url = url;
        l10n_util::WrapStringWithLTRFormatting(&localized_url);
        return localized_url;
      }
      return url;
    }
    case IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN: {  // Username.
      std::wstring username = GetPasswordFormAt(row)->username_value;
      l10n_util::AdjustStringForLocaleDirection(username, &username);
      return username;
    }
    default:
      NOTREACHED() << "Invalid column.";
      return std::wstring();
  }
}

int PasswordsTableModel::CompareValues(int row1, int row2,
                                       int column_id) {
  if (column_id == IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN) {
    return saved_signons_[row1]->display_url.Compare(
        saved_signons_[row2]->display_url, GetCollator());
  }
  return TableModel::CompareValues(row1, row2, column_id);
}

void PasswordsTableModel::SetObserver(TableModelObserver* observer) {
  observer_ = observer;
}

void PasswordsTableModel::GetAllSavedLoginsForProfile() {
  DCHECK(!pending_login_query_);
  pending_login_query_ = web_data_service()->GetAllAutofillableLogins(this);
}

void PasswordsTableModel::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK_EQ(pending_login_query_, h);
  pending_login_query_ = NULL;

  if (!result)
    return;

  DCHECK(result->GetType() == PASSWORD_RESULT);

  // Get the result from the database into a useable form.
  const WDResult<std::vector<PasswordForm*> >* r =
      static_cast<const WDResult<std::vector<PasswordForm*> >*>(result);
  std::vector<PasswordForm*> rows = r->GetValue();
  STLDeleteElements<PasswordRows>(&saved_signons_);
  saved_signons_.resize(rows.size(), NULL);
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  for (size_t i = 0; i < rows.size(); ++i) {
    saved_signons_[i] = new PasswordRow(
        gfx::SortedDisplayURL(rows[i]->origin, languages), rows[i]);
  }
  if (observer_)
    observer_->OnModelChanged();
  if (row_count_observer_)
    row_count_observer_->OnRowCountChanged(RowCount());
}

PasswordForm* PasswordsTableModel::GetPasswordFormAt(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return saved_signons_[row]->form.get();
}

void PasswordsTableModel::ForgetAndRemoveSignon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  PasswordRows::iterator target_iter = saved_signons_.begin() + row;
  // Remove from DB, memory, and vector.
  PasswordRow* password_row = *target_iter;
  web_data_service()->RemoveLogin(*(password_row->form.get()));
  delete password_row;
  saved_signons_.erase(target_iter);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
  if (row_count_observer_)
    row_count_observer_->OnRowCountChanged(RowCount());
}

void PasswordsTableModel::ForgetAndRemoveAllSignons() {
  PasswordRows::iterator iter = saved_signons_.begin();
  while (iter != saved_signons_.end()) {
    // Remove from DB, memory, and vector.
    PasswordRow* row = *iter;
    web_data_service()->RemoveLogin(*(row->form.get()));
    delete row;
    iter = saved_signons_.erase(iter);
  }
  if (observer_)
    observer_->OnModelChanged();
  if (row_count_observer_)
    row_count_observer_->OnRowCountChanged(RowCount());
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsTableModel, private
void PasswordsTableModel::CancelLoginsQuery() {
  if (pending_login_query_) {
    web_data_service()->CancelRequest(pending_login_query_);
    pending_login_query_ = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView, public
PasswordsPageView::PasswordsPageView(Profile* profile)
    : OptionsPageView(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_button_(
          this,
          l10n_util::GetString(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON),
          l10n_util::GetString(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON))),
      ALLOW_THIS_IN_INITIALIZER_LIST(remove_button_(
          this,
          l10n_util::GetString(IDS_PASSWORDS_PAGE_VIEW_REMOVE_BUTTON))),
      ALLOW_THIS_IN_INITIALIZER_LIST(remove_all_button_(
          this,
          l10n_util::GetString(IDS_PASSWORDS_PAGE_VIEW_REMOVE_ALL_BUTTON))),
      table_model_(profile),
      table_view_(NULL),
      current_selected_password_(NULL) {
}

void PasswordsPageView::OnSelectionChanged() {
  bool has_selection = table_view_->SelectedRowCount() > 0;
  remove_button_.SetEnabled(has_selection);

  PasswordForm* selected = NULL;
  if (has_selection) {
    views::TableSelectionIterator iter = table_view_->SelectionBegin();
    selected = table_model_.GetPasswordFormAt(*iter);
    DCHECK(++iter == table_view_->SelectionEnd());
  }

  if (selected != current_selected_password_) {
    // Reset the password related views.
    show_button_.SetLabel(
        l10n_util::GetString(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON));
    show_button_.SetEnabled(has_selection);
    password_label_.SetText(std::wstring());

    current_selected_password_ = selected;
  }
}

void PasswordsPageView::ButtonPressed(views::Button* sender) {
  // Close will result in our destruction.
  if (sender == &remove_all_button_) {
    table_model_.ForgetAndRemoveAllSignons();
    return;
  }

  // The following require a selection (and only one, since table is single-
  // select only).
  views::TableSelectionIterator iter = table_view_->SelectionBegin();
  int row = *iter;
  PasswordForm* selected = table_model_.GetPasswordFormAt(row);
  DCHECK(++iter == table_view_->SelectionEnd());

  if (sender == &remove_button_) {
    table_model_.ForgetAndRemoveSignon(row);
  } else if (sender == &show_button_) {
    if (password_label_.GetText().length() == 0) {
      password_label_.SetText(selected->password_value);
      show_button_.SetLabel(
          l10n_util::GetString(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON));
    } else {
      password_label_.SetText(L"");
      show_button_.SetLabel(
          l10n_util::GetString(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON));
    }
  } else {
    NOTREACHED() << "Invalid button.";
  }
}

void PasswordsPageView::OnRowCountChanged(size_t rows) {
  remove_all_button_.SetEnabled(rows > 0);
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView, protected
void PasswordsPageView::InitControlLayout() {
  SetupButtonsAndLabels();
  SetupTable();

  // Do the layout thing.
  const int top_column_set_id = 0;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // Design the grid.
  ColumnSet* column_set = layout->AddColumnSet(top_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Fill the grid.
  layout->StartRow(0, top_column_set_id);
  layout->AddView(table_view_, 1, 8, GridLayout::FILL,
                  GridLayout::FILL);
  layout->AddView(&remove_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              kRelatedControlVerticalSpacing);
  layout->SkipColumns(1);
  layout->AddView(&remove_all_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              kRelatedControlVerticalSpacing);
  layout->SkipColumns(1);
  layout->AddView(&show_button_);
  layout->StartRowWithPadding(0, top_column_set_id, 0,
                              kRelatedControlVerticalSpacing);
  layout->SkipColumns(1);
  layout->AddView(&password_label_);
  layout->AddPaddingRow(1, 0);

  // Ask the database for saved password data.
  table_model_.GetAllSavedLoginsForProfile();
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView, private
void PasswordsPageView::SetupButtonsAndLabels() {
  // Disable all buttons in the first place.
  show_button_.SetParentOwned(false);
  show_button_.SetEnabled(false);

  remove_button_.SetParentOwned(false);
  remove_button_.SetEnabled(false);

  remove_all_button_.SetParentOwned(false);
  remove_all_button_.SetEnabled(false);

  password_label_.SetParentOwned(false);
}

void PasswordsPageView::SetupTable() {
  // Tell the table model we are concern about how many rows it has.
  table_model_.set_row_count_observer(this);

  // Creates the different columns for the table.
  // The float resize values are the result of much tinkering.
  std::vector<TableColumn> columns;
  columns.push_back(TableColumn(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN,
                                       TableColumn::LEFT, -1, 0.55f));
  columns.back().sortable = true;
  columns.push_back(TableColumn(
      IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN, TableColumn::LEFT,
      -1, 0.37f));
  columns.back().sortable = true;
  table_view_ = new views::TableView(&table_model_, columns, views::TEXT_ONLY,
                                     true, true, true);
  // Make the table initially sorted by host.
  views::TableView::SortDescriptors sort;
  sort.push_back(views::TableView::SortDescriptor(
      IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN, true));
  table_view_->SetSortDescriptors(sort);
  table_view_->SetObserver(this);
}
