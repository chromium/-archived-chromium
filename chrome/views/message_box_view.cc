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

#include "chrome/views/message_box_view.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/controller.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/client_view.h"

#include "generated_resources.h"

static const int kDefaultMessageWidth = 320;

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, public:

MessageBoxView::MessageBoxView(int dialog_flags,
                               const std::wstring& message,
                               const std::wstring& default_prompt,
                               int message_width)
    : message_label_(new ChromeViews::Label(message)),
      prompt_field_(NULL),
      icon_(NULL),
      check_box_(NULL),
      message_width_(message_width),
      focus_grabber_factory_(this) {
  Init(dialog_flags, default_prompt);
}

MessageBoxView::MessageBoxView(int dialog_flags,
                               const std::wstring& message,
                               const std::wstring& default_prompt)
    : message_label_(new ChromeViews::Label(message)),
      prompt_field_(NULL),
      icon_(NULL),
      check_box_(NULL),
      message_width_(kDefaultMessageWidth),
      focus_grabber_factory_(this) {
  Init(dialog_flags, default_prompt);
}

std::wstring MessageBoxView::GetInputText() {
  if (prompt_field_)
    return prompt_field_->GetText();
  return EmptyWString();
}

bool MessageBoxView::IsCheckBoxSelected() {
  if (check_box_ == NULL)
    return false;
  return check_box_->IsSelected();
}

void MessageBoxView::SetIcon(const SkBitmap& icon) {
  if (!icon_)
    icon_ = new ChromeViews::ImageView();
  icon_->SetImage(icon);
  icon_->SetBounds(0, 0, icon.width(), icon.height());
  ResetLayoutManager();
}

void MessageBoxView::SetCheckBoxLabel(const std::wstring& label) {
  if (!check_box_)
    check_box_ = new ChromeViews::CheckBox(label);
  else
    check_box_->SetLabel(label);
  ResetLayoutManager();
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, ChromeViews::View overrides:

void MessageBoxView::ViewHierarchyChanged(bool is_add,
                                          ChromeViews::View* parent,
                                          ChromeViews::View* child) {
  if (child == this && is_add) {
    if (prompt_field_)
      prompt_field_->SelectAll();
    MessageLoop::current()->PostTask(FROM_HERE,
        focus_grabber_factory_.NewRunnableMethod(
            &MessageBoxView::FocusFirstFocusableControl));
  }
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, private:

void MessageBoxView::FocusFirstFocusableControl() {
  if (prompt_field_)
    prompt_field_->RequestFocus();
  else if (check_box_)
    check_box_->RequestFocus();
  else
    RequestFocus();
}

void MessageBoxView::Init(int dialog_flags,
                          const std::wstring& default_prompt) {
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(ChromeViews::Label::ALIGN_LEFT);

  if (dialog_flags & kFlagHasPromptField) {
    prompt_field_ = new ChromeViews::TextField;
    prompt_field_->SetText(default_prompt);
  }

  ResetLayoutManager();
}

void MessageBoxView::ResetLayoutManager() {
  using ChromeViews::GridLayout;
  using ChromeViews::ColumnSet;

  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  CSize icon_size;
  if (icon_)
    icon_->GetPreferredSize(&icon_size);

  // Add the column set for the message displayed at the top of the dialog box.
  // And an icon, if one has been set.
  const int message_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(message_column_view_set_id);
  if (icon_) {
    column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                          GridLayout::FIXED, icon_size.cx, icon_size.cy);
    column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  }
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, message_width_, 0);

  // Column set for prompt textfield, if one has been set.
  const int textfield_column_view_set_id = 1;
  if (prompt_field_) {
    column_set = layout->AddColumnSet(textfield_column_view_set_id);
    if (icon_) {
      column_set->AddPaddingColumn(0,
          icon_size.cx + kUnrelatedControlHorizontalSpacing);
    }
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
  }

  // Column set for checkbox, if one has been set.
  const int checkbox_column_view_set_id = 2;
  if (check_box_) {
    column_set = layout->AddColumnSet(checkbox_column_view_set_id);
    if (icon_) {
      column_set->AddPaddingColumn(0,
          icon_size.cx + kUnrelatedControlHorizontalSpacing);
    }
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
  }

  layout->StartRow(0, message_column_view_set_id);
  if (icon_)
    layout->AddView(icon_);

  layout->AddView(message_label_);

  if (prompt_field_) {
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, textfield_column_view_set_id);
    layout->AddView(prompt_field_);
  }

  if (check_box_) {
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, checkbox_column_view_set_id);
    layout->AddView(check_box_);
  }

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}
