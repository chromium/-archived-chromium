// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/logging_about_dialog.h"

#include "chrome/browser/views/standard_layout.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/text_field.h"
#include "chrome/views/window.h"

LoggingAboutDialog::LoggingAboutDialog() {
}

LoggingAboutDialog::~LoggingAboutDialog() {
}

void LoggingAboutDialog::SetupControls() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  static const int first_column_set = 1;
  views::ColumnSet* button_set = layout->AddColumnSet(first_column_set);
  SetupButtonColumnSet(button_set);

  text_field_ = new views::TextField(static_cast<views::TextField::StyleFlags>(
                                     views::TextField::STYLE_MULTILINE));
  text_field_->SetReadOnly(true);

  // TODO(brettw): We may want to add this in the future. It can't be called
  // from here, though, since the hwnd for the field hasn't been created yet.
  //
  // This raises the maximum number of chars from 32K to some large maximum,
  // probably 2GB. 32K is not nearly enough for our use-case.
  //SendMessageW(text_field_->GetNativeComponent(), EM_SETLIMITTEXT, 0, 0);

  static const int text_column_set = 2;
  views::ColumnSet* column_set = layout->AddColumnSet(text_column_set);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 100.0f,
                        views::GridLayout::FIXED, 0, 0);

  layout->StartRow(0, first_column_set);
  AddButtonControlsToLayout(layout);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1.0f, text_column_set);
  layout->AddView(text_field_);
}

void LoggingAboutDialog::AppendText(const std::wstring& text) {
  text_field_->AppendText(text);
}

gfx::Size LoggingAboutDialog::GetPreferredSize() {
  return gfx::Size(800, 400);
}

views::View* LoggingAboutDialog::GetContentsView() {
  return this;
}

int LoggingAboutDialog::GetDialogButtons() const {
  // Don't want OK or Cancel.
  return 0;
}

std::wstring LoggingAboutDialog::GetWindowTitle() const {
  return L"about:network";
}

bool LoggingAboutDialog::CanResize() const {
  return true;
}
