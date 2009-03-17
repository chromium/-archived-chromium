// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/message_box_view.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/controls/button/checkbox.h"
#include "chrome/views/window/client_view.h"
#include "grit/generated_resources.h"

static const int kDefaultMessageWidth = 320;

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, public:

MessageBoxView::MessageBoxView(int dialog_flags,
                               const std::wstring& message,
                               const std::wstring& default_prompt,
                               int message_width)
    : message_label_(new views::Label(message)),
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
    : message_label_(new views::Label(message)),
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
    icon_ = new views::ImageView();
  icon_->SetImage(icon);
  icon_->SetBounds(0, 0, icon.width(), icon.height());
  ResetLayoutManager();
}

void MessageBoxView::SetCheckBoxLabel(const std::wstring& label) {
  if (!check_box_)
    check_box_ = new views::CheckBox(label);
  else
    check_box_->SetLabel(label);
  ResetLayoutManager();
}

void MessageBoxView::SetCheckBoxSelected(bool selected) {
  if (!check_box_)
    return;
  check_box_->SetIsSelected(selected);
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, views::View overrides:

void MessageBoxView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (child == this && is_add) {
    if (prompt_field_)
      prompt_field_->SelectAll();
  }
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, private:

void MessageBoxView::Init(int dialog_flags,
                          const std::wstring& default_prompt) {
  message_label_->SetMultiLine(true);
  if (dialog_flags & kAutoDetectAlignment) {
    // Determine the alignment and directionality based on the first character
    // with strong directionality.
    l10n_util::TextDirection direction =
        l10n_util::GetFirstStrongCharacterDirection(message_label_->GetText());
    views::Label::Alignment alignment;
    if (direction == l10n_util::RIGHT_TO_LEFT)
      alignment = views::Label::ALIGN_RIGHT;
    else
      alignment = views::Label::ALIGN_LEFT;
    // In addition, we should set the RTL alignment mode as
    // AUTO_DETECT_ALIGNMENT so that the alignment will not be flipped around
    // in RTL locales.
    message_label_->SetRTLAlignmentMode(views::Label::AUTO_DETECT_ALIGNMENT);
    message_label_->SetHorizontalAlignment(alignment);
  } else {
    message_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  }

  if (dialog_flags & kFlagHasPromptField) {
    prompt_field_ = new views::TextField;
    prompt_field_->SetText(default_prompt);
  }

  ResetLayoutManager();
}

void MessageBoxView::ResetLayoutManager() {
  using views::GridLayout;
  using views::ColumnSet;

  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  gfx::Size icon_size;
  if (icon_)
    icon_size = icon_->GetPreferredSize();

  // Add the column set for the message displayed at the top of the dialog box.
  // And an icon, if one has been set.
  const int message_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(message_column_view_set_id);
  if (icon_) {
    column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                          GridLayout::FIXED, icon_size.width(),
                          icon_size.height());
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
          icon_size.width() + kUnrelatedControlHorizontalSpacing);
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
          icon_size.width() + kUnrelatedControlHorizontalSpacing);
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
