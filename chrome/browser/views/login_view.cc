// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/views/login_view.h"

#include "base/message_loop.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/label.h"
#include "chrome/views/text_field.h"
#include "chrome/views/widget/root_view.h"
#include "grit/generated_resources.h"

namespace views {

static const int kMessageWidth = 320;
static const int kTextFieldStackHorizontalSpacing = 30;

///////////////////////////////////////////////////////////////////////////////
// LoginView, public:

LoginView::LoginView(const std::wstring& explanation)
    : username_field_(new TextField),
      password_field_(new TextField(TextField::STYLE_PASSWORD)),
      username_label_(new Label(
          l10n_util::GetString(IDS_LOGIN_DIALOG_USERNAME_FIELD))),
      password_label_(new Label(
          l10n_util::GetString(IDS_LOGIN_DIALOG_PASSWORD_FIELD))),
      message_label_(new Label(explanation)),
      focus_grabber_factory_(this),
      login_model_(NULL) {
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);

  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // Add the column set for the information message at the top of the dialog
  // box.
  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, kMessageWidth, 0);

  // Add the column set for the user name and password fields and labels.
  const int labels_column_set_id = 1;
  column_set = layout->AddColumnSet(labels_column_set_id);
  column_set->AddPaddingColumn(0, kTextFieldStackHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kTextFieldStackHorizontalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(message_label_);

  layout->AddPaddingRow(0, kUnrelatedControlLargeVerticalSpacing);

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(username_label_);
  layout->AddView(username_field_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(password_label_);
  layout->AddView(password_field_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
}

LoginView::~LoginView() {
  if (login_model_)
    login_model_->SetObserver(NULL);
}

std::wstring LoginView::GetUsername() {
  return username_field_->GetText();
}

std::wstring LoginView::GetPassword() {
  return password_field_->GetText();
}

void LoginView::SetModel(LoginModel* model) {
  login_model_ = model;
  if (login_model_)
    login_model_->SetObserver(this);
}
///////////////////////////////////////////////////////////////////////////////
// LoginView, views::View, views::LoginModelObserver overrides:

void LoginView::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (is_add && child == this) {
    MessageLoop::current()->PostTask(FROM_HERE,
        focus_grabber_factory_.NewRunnableMethod(&LoginView::FocusFirstField));
  }
}

void LoginView::OnAutofillDataAvailable(const std::wstring& username,
                                        const std::wstring& password) {
  if (username_field_->GetText().empty()) {
    username_field_->SetText(username);
    password_field_->SetText(password);
    username_field_->SelectAll();
  }
}

///////////////////////////////////////////////////////////////////////////////
// LoginView, private:

void LoginView::FocusFirstField() {
  username_field_->RequestFocus();
}

}  // namespace
