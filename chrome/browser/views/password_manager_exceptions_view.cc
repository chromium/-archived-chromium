// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/password_manager_exceptions_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/background.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/native_button.h"
#include "grit/generated_resources.h"

using views::ColumnSet;
using views::GridLayout;

// We can only have one PasswordManagerExceptionsView at a time.
static PasswordManagerExceptionsView* instance_ = NULL;

static const int kDefaultWindowWidth = 530;
static const int kDefaultWindowHeight = 240;

////////////////////////////////////////////////////////////////////
// PasswordManagerExceptionsTableModel
PasswordManagerExceptionsTableModel::PasswordManagerExceptionsTableModel(
    Profile* profile) : PasswordManagerTableModel(profile) {
}

PasswordManagerExceptionsTableModel::~PasswordManagerExceptionsTableModel() {
}

std::wstring PasswordManagerExceptionsTableModel::GetText(int row, int col_id) {
  DCHECK_EQ(col_id, IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN);
  return PasswordManagerTableModel::GetText(row, col_id);
}

int PasswordManagerExceptionsTableModel::CompareValues(int row1, int row2,
                                                       int col_id) {
  DCHECK_EQ(col_id, IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN);
  return PasswordManagerTableModel::CompareValues(row1, row2, col_id);
}

void PasswordManagerExceptionsTableModel::GetAllExceptionsForProfile() {
  DCHECK(!pending_login_query_);
  pending_login_query_ = web_data_service()->GetAllLogins(this);
}

void PasswordManagerExceptionsTableModel::OnWebDataServiceRequestDone(
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
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i]->blacklisted_by_user) {
      saved_signons_.push_back(new PasswordRow(
          gfx::SortedDisplayURL(rows[i]->origin, languages), rows[i]));
    }
  }
  if (observer_)
    observer_->OnModelChanged();
}

//////////////////////////////////////////////////////////////////////
// PasswordManagerExceptionsView

// static
void PasswordManagerExceptionsView::Show(Profile* profile) {
  DCHECK(profile);
  if (!instance_) {
    instance_ = new PasswordManagerExceptionsView(profile);

    // manager is owned by the dialog window, so Close() will delete it.
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible()) {
    instance_->window()->Show();
  } else {
    instance_->window()->Activate();
  }
}

PasswordManagerExceptionsView::PasswordManagerExceptionsView(Profile* profile)
    : remove_button_(l10n_util::GetString(
          IDS_PASSWORD_MANAGER_EXCEPTIONS_VIEW_REMOVE_BUTTON)),
      remove_all_button_(l10n_util::GetString(
          IDS_PASSWORD_MANAGER_EXCEPTIONS_VIEW_REMOVE_ALL_BUTTON)),
      table_model_(profile) {
  Init();
}

void PasswordManagerExceptionsView::SetupTable() {
  // Creates the different columns for the table.
  // The float resize values are the result of much tinkering.
  std::vector<views::TableColumn> columns;
  columns.push_back(views::TableColumn(
      IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN,
      views::TableColumn::LEFT, -1, 0.55f));
  columns.back().sortable = true;
  table_view_ = new views::TableView(&table_model_, columns, views::TEXT_ONLY,
                                     true, true, true);
  // Make the table initially sorted by host.
  views::TableView::SortDescriptors sort;
  sort.push_back(views::TableView::SortDescriptor(
      IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN, true));
  table_view_->SetSortDescriptors(sort);
  table_view_->SetObserver(this);
}

void PasswordManagerExceptionsView::SetupButtons() {
  // Tell View not to delete class stack allocated views.

  remove_button_.SetParentOwned(false);
  remove_button_.SetListener(this);
  remove_button_.SetEnabled(false);

  remove_all_button_.SetParentOwned(false);
  remove_all_button_.SetListener(this);
}

void PasswordManagerExceptionsView::Init() {
  // Configure the background and view elements (buttons, labels, table).
  SetupButtons();
  SetupTable();

  // Do the layout thing.
  const int column_set_id = 0;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // Design the grid.
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, 300, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Fill the grid.
  layout->StartRow(0.05f, column_set_id);
  layout->AddView(table_view_);
  layout->AddView(&remove_button_);

  // Ask the database for exception data.
  table_model_.GetAllExceptionsForProfile();
}

PasswordManagerExceptionsView::~PasswordManagerExceptionsView() {
}

void PasswordManagerExceptionsView::Layout() {
  GetLayoutManager()->Layout(this);

  // Manually lay out the Remove All button in the same row as
  // the close button.
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);
  gfx::Size prefsize = remove_all_button_.GetPreferredSize();
  int button_y =
      parent_bounds.bottom() - prefsize.height() - kButtonVEdgeMargin;
  remove_all_button_.SetBounds(kPanelHorizMargin, button_y, prefsize.width(),
                               prefsize.height());
}

gfx::Size PasswordManagerExceptionsView::GetPreferredSize() {
  return gfx::Size(kDefaultWindowWidth, kDefaultWindowHeight);
}

void PasswordManagerExceptionsView::ViewHierarchyChanged(bool is_add,
                                                         views::View* parent,
                                                         views::View* child) {
  if (child == this) {
    // Add and remove the Remove All button from the ClientView's hierarchy.
    if (is_add) {
      parent->AddChildView(&remove_all_button_);
    } else {
      parent->RemoveChildView(&remove_all_button_);
    }
  }
}

void PasswordManagerExceptionsView::OnSelectionChanged() {
  bool has_selection = table_view_->SelectedRowCount() > 0;
  remove_button_.SetEnabled(has_selection);
}

int PasswordManagerExceptionsView::GetDialogButtons() const {
  return DIALOGBUTTON_CANCEL;
}

std::wstring PasswordManagerExceptionsView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_PASSWORD_MANAGER_EXCEPTIONS_VIEW_TITLE);
}

void PasswordManagerExceptionsView::ButtonPressed(views::NativeButton* sender) {
  DCHECK(window());
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
  } else {
    NOTREACHED() << "Invalid button.";
  }
}

void PasswordManagerExceptionsView::WindowClosing() {
  // The table model will be deleted before the table view, so detach it.
  table_view_->SetModel(NULL);

  // Clear the static instance so the next time Show() is called, a new
  // instance is created.
  instance_ = NULL;
}

views::View* PasswordManagerExceptionsView::GetContentsView() {
  return this;
}
