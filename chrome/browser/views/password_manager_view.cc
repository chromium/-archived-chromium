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

#include "base/string_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/views/password_manager_view.h"
#include "chrome/views/background.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/native_button.h"
#include "webkit/glue/password_form.h"

#include "generated_resources.h"

using ChromeViews::ColumnSet;
using ChromeViews::GridLayout;

// We can only have one PasswordManagerView at a time.
static PasswordManagerView* instance_ = NULL;

static const int kDefaultWindowWidth = 530;
static const int kDefaultWindowHeight = 240;

////////////////////////////////////////////////////////////////////////////////
// MultiLabelButtons
//
MultiLabelButtons::MultiLabelButtons(const std::wstring& label,
                                     const std::wstring& alt_label)
    : NativeButton(label),
      label_(label),
      alt_label_(alt_label),
      pref_size_(-1, -1) {
}

void MultiLabelButtons::GetPreferredSize(CSize *out) {
  if (pref_size_.cx == -1 && pref_size_.cy == -1) {
    // Let's compute our preferred size.
    std::wstring current_label = GetLabel();
    SetLabel(label_);
    NativeButton::GetPreferredSize(&pref_size_);
    SetLabel(alt_label_);
    CSize alt_pref_size;
    NativeButton::GetPreferredSize(&alt_pref_size);
    // Revert to the original label.
    SetLabel(current_label);
    pref_size_.cx = std::max(pref_size_.cx, alt_pref_size.cx);
    pref_size_.cy = std::max(pref_size_.cy, alt_pref_size.cy);
  }
  *out = pref_size_;
}

////////////////////////////////////////////////////////////////////
// PasswordManagerTableModel
PasswordManagerTableModel::PasswordManagerTableModel(
    WebDataService* profile_web_data_service)
    : saved_signons_deleter_(&saved_signons_),
      observer_(NULL),
      pending_login_query_(NULL),
      web_data_service_(profile_web_data_service) {
  DCHECK(web_data_service_);
}

PasswordManagerTableModel::~PasswordManagerTableModel() {
  CancelLoginsQuery();
}

int PasswordManagerTableModel::RowCount() {
  return static_cast<int>(saved_signons_.size());
}

std::wstring PasswordManagerTableModel::GetText(int row,
                                                int col_id) {
  PasswordForm* signon = GetPasswordFormAt(row);
  DCHECK(signon);
  switch (col_id) {
    case IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN:  // Site.
      return UTF8ToWide(signon->origin.spec());
    case IDS_PASSWORD_MANAGER_VIEW_USERNAME_COLUMN:  // Username.
      return signon->username_value;
    default:
      NOTREACHED() << "Invalid column.";
      return std::wstring();
  }
}

void PasswordManagerTableModel::SetObserver(
    ChromeViews::TableModelObserver* observer) {
  observer_ = observer;
}

void PasswordManagerTableModel::GetAllSavedLoginsForProfile() {
  DCHECK(!pending_login_query_);
  pending_login_query_ = web_data_service_->GetAllAutofillableLogins(this);
}

void PasswordManagerTableModel::OnWebDataServiceRequestDone(
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
  // Copy vector of *pointers* to saved_logins_, thus taking ownership of the
  // PasswordForm's in the result.
  saved_signons_ = r->GetValue();
  if (observer_)
    observer_->OnModelChanged();
}

void PasswordManagerTableModel::CancelLoginsQuery() {
  if (pending_login_query_) {
    web_data_service_->CancelRequest(pending_login_query_);
    pending_login_query_ = NULL;
  }
}

PasswordForm* PasswordManagerTableModel::GetPasswordFormAt(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return saved_signons_[row];
}

void PasswordManagerTableModel::ForgetAndRemoveSignon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  PasswordForms::iterator target_iter = saved_signons_.begin() + row;
  // Remove from DB, memory, and vector.
  web_data_service_->RemoveLogin(**target_iter);
  delete *target_iter;
  saved_signons_.erase(target_iter);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void PasswordManagerTableModel::ForgetAndRemoveAllSignons() {
  PasswordForms::iterator iter = saved_signons_.begin();
  while (iter != saved_signons_.end()) {
    web_data_service_->RemoveLogin(**iter);
    delete *iter;
    iter = saved_signons_.erase(iter);
  }
  if (observer_)
    observer_->OnModelChanged();
}

//////////////////////////////////////////////////////////////////////
// PasswordManagerView

// static
void PasswordManagerView::Show(Profile* profile) {
  DCHECK(profile);
  if (!instance_) {
    instance_ = new PasswordManagerView(profile);

    // manager is owned by the dialog window, so Close() will delete it.
    ChromeViews::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible()) {
    instance_->window()->Show();
  } else {
    instance_->window()->Activate();
  }
}

PasswordManagerView::PasswordManagerView(Profile* profile)
    : show_button_(
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_SHOW_BUTTON),
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_HIDE_BUTTON)),
      remove_button_(l10n_util::GetString(
          IDS_PASSWORD_MANAGER_VIEW_REMOVE_BUTTON)),
      remove_all_button_(l10n_util::GetString(
          IDS_PASSWORD_MANAGER_VIEW_REMOVE_ALL_BUTTON)),
      table_model_(profile->GetWebDataService(Profile::EXPLICIT_ACCESS)) {
  Init();
}

void PasswordManagerView::SetupTable() {
  // Creates the different columns for the table.
  // The float resize values are the result of much tinkering.
  std::vector<ChromeViews::TableColumn> columns;
  columns.push_back(
      ChromeViews::TableColumn(
          IDS_PASSWORD_MANAGER_VIEW_SITE_COLUMN,
          ChromeViews::TableColumn::LEFT, -1, 0.55f));
  columns.push_back(
      ChromeViews::TableColumn(
          IDS_PASSWORD_MANAGER_VIEW_USERNAME_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0.37f));
  table_view_ = new ChromeViews::TableView(&table_model_,
                                           columns,
                                           ChromeViews::TEXT_ONLY,
                                           true, true, true);
  table_view_->SetObserver(this);
}

void PasswordManagerView::SetupButtonsAndLabels() {
  // Tell View not to delete class stack allocated views.

  show_button_.SetParentOwned(false);
  show_button_.SetListener(this);
  show_button_.SetEnabled(false);

  remove_button_.SetParentOwned(false);
  remove_button_.SetListener(this);
  remove_button_.SetEnabled(false);

  remove_all_button_.SetParentOwned(false);
  remove_all_button_.SetListener(this);

  password_label_.SetParentOwned(false);
}

void PasswordManagerView::Init() {
  // Configure the background and view elements (buttons, labels, table).
  SetupButtonsAndLabels();
  SetupTable();

  // Do the layout thing.
  const int top_column_set_id = 0;
  const int lower_column_set_id = 1;
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // Design the grid.
  ColumnSet* column_set = layout->AddColumnSet(top_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, 300, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(lower_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes(0, 2, -1);

  // Fill the grid.
  layout->StartRow(0.05f, top_column_set_id);
  layout->AddView(table_view_, 1, 3);
  layout->AddView(&remove_button_);
  layout->StartRow(0.05f, top_column_set_id);
  layout->SkipColumns(1);
  layout->AddView(&show_button_);
  layout->StartRow(0.80f, top_column_set_id);
  layout->SkipColumns(1);
  layout->AddView(&password_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Ask the database for saved password data.
  table_model_.GetAllSavedLoginsForProfile();
}

PasswordManagerView::~PasswordManagerView() {
}

void PasswordManagerView::Layout() {
  GetLayoutManager()->Layout(this);

  // Manually lay out the Remove All button in the same row as
  // the close button.
  CRect parent_bounds;
  GetParent()->GetLocalBounds(&parent_bounds, false);
  CSize prefsize;
  remove_all_button_.GetPreferredSize(&prefsize);
  int button_y = parent_bounds.bottom - prefsize.cy - kButtonVEdgeMargin;
  remove_all_button_.SetBounds(kPanelHorizMargin, button_y, prefsize.cx,
                               prefsize.cy);
}

void PasswordManagerView::GetPreferredSize(CSize* out) {
  out->cx = kDefaultWindowWidth;
  out->cy = kDefaultWindowHeight;
}

void PasswordManagerView::ViewHierarchyChanged(bool is_add,
                                               ChromeViews::View* parent,
                                               ChromeViews::View* child) {
  if (child == this) {
    // Add and remove the Remove All button from the ClientView's hierarchy.
    if (is_add) {
      parent->AddChildView(&remove_all_button_);
    } else {
      parent->RemoveChildView(&remove_all_button_);
    }
  }
}

void PasswordManagerView::OnSelectionChanged() {
  bool has_selection = table_view_->SelectedRowCount() > 0;
  remove_button_.SetEnabled(has_selection);
  // Reset the password related views.
  show_button_.SetLabel(
      l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_SHOW_BUTTON));
  show_button_.SetEnabled(has_selection);
  password_label_.SetText(std::wstring());
}

int PasswordManagerView::GetDialogButtons() const {
  return DIALOGBUTTON_CANCEL;
}

bool PasswordManagerView::CanResize() const {
  return true;
}

bool PasswordManagerView::CanMaximize() const {
  return false;
}

bool PasswordManagerView::IsAlwaysOnTop() const {
  return false;
}

bool PasswordManagerView::HasAlwaysOnTopMenu() const {
  return false;
}

std::wstring PasswordManagerView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_TITLE);
}

void PasswordManagerView::ButtonPressed(ChromeViews::NativeButton* sender) {
  DCHECK(window());
  // Close will result in our destruction.
  if (sender == &remove_all_button_) {
    table_model_.ForgetAndRemoveAllSignons();
    return;
  }

  // The following require a selection (and only one, since table is single-
  // select only).
  ChromeViews::TableSelectionIterator iter = table_view_->SelectionBegin();
  int row = *iter;
  PasswordForm* selected = table_model_.GetPasswordFormAt(row);
  DCHECK(++iter == table_view_->SelectionEnd());

  if (sender == &remove_button_) {
    table_model_.ForgetAndRemoveSignon(row);
  } else if (sender == &show_button_) {
    if (password_label_.GetText().length() == 0) {
      password_label_.SetText(selected->password_value);
      show_button_.SetLabel(
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_HIDE_BUTTON));
    } else {
      password_label_.SetText(L"");
      show_button_.SetLabel(
          l10n_util::GetString(IDS_PASSWORD_MANAGER_VIEW_SHOW_BUTTON));
    }
  } else {
    NOTREACHED() << "Invalid button.";
  }
}

void PasswordManagerView::WindowClosing() {
  // The table model will be deleted before the table view, so detach it.
  table_view_->SetModel(NULL);

  // Clear the static instance so the next time Show() is called, a new
  // instance is created.
  instance_ = NULL;
}

ChromeViews::View* PasswordManagerView::GetContentsView() {
  return this;
}
