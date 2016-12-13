// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/edit_search_engine_dialog.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/search_engines/template_url.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/label.h"
#include "views/controls/image_view.h"
#include "views/controls/table/table_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

using views::GridLayout;
using views::ImageView;
using views::Textfield;


namespace {
// Converts a URL as understood by TemplateURL to one appropriate for display
// to the user.
std::wstring GetDisplayURL(const TemplateURL& turl) {
  return turl.url() ? turl.url()->DisplayURL() : std::wstring();
}
}  // namespace

namespace browser {

void EditSearchEngine(gfx::NativeWindow parent,
                      const TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile) {
  EditSearchEngineDialog::Show(parent, template_url, delegate, profile);
}

}  // namespace browser

EditSearchEngineDialog::EditSearchEngineDialog(
    const TemplateURL* template_url,
    EditSearchEngineControllerDelegate* delegate,
    Profile* profile)
    : controller_(new EditSearchEngineController(template_url,
                                                 delegate,
                                                 profile)) {
  Init();
}

// static
void EditSearchEngineDialog::Show(gfx::NativeWindow parent,
                                  const TemplateURL* template_url,
                                  EditSearchEngineControllerDelegate* delegate,
                                  Profile* profile) {
  EditSearchEngineDialog* contents =
      new EditSearchEngineDialog(template_url, delegate, profile);
  // Window interprets an empty rectangle as needing to query the content for
  // the size as well as centering relative to the parent.
  views::Window::CreateChromeWindow(parent, gfx::Rect(), contents);
  contents->window()->Show();
  contents->GetDialogClientView()->UpdateDialogButtons();
  contents->title_tf_->SelectAll();
  contents->title_tf_->RequestFocus();
}

bool EditSearchEngineDialog::IsModal() const {
  return true;
}

std::wstring EditSearchEngineDialog::GetWindowTitle() const {
  return l10n_util::GetString(controller_->template_url() ?
      IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE :
      IDS_SEARCH_ENGINES_EDITOR_NEW_WINDOW_TITLE);
}

bool EditSearchEngineDialog::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return (controller_->IsKeywordValid(keyword_tf_->text()) &&
            controller_->IsTitleValid(title_tf_->text()) &&
            controller_->IsURLValid(url_tf_->text()));
  }
  return true;
}

bool EditSearchEngineDialog::Cancel() {
  controller_->CleanUpCancelledAdd();
  return true;
}

bool EditSearchEngineDialog::Accept() {
  controller_->AcceptAddOrEdit(title_tf_->text(), keyword_tf_->text(),
                               url_tf_->text());
  return true;
}

views::View* EditSearchEngineDialog::GetContentsView() {
  return this;
}

void EditSearchEngineDialog::ContentsChanged(Textfield* sender,
                                             const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
  UpdateImageViews();
}

bool EditSearchEngineDialog::HandleKeystroke(
    Textfield* sender,
    const views::Textfield::Keystroke& key) {
  return false;
}

void EditSearchEngineDialog::Init() {
  // Create the views we'll need.
  if (controller_->template_url()) {
    title_tf_ =
        CreateTextfield(controller_->template_url()->short_name(), false);
    keyword_tf_ = CreateTextfield(controller_->template_url()->keyword(), true);
    url_tf_ =
        CreateTextfield(GetDisplayURL(*controller_->template_url()), false);
    // We don't allow users to edit prepopulate URLs. This is done as
    // occasionally we need to update the URL of prepopulated TemplateURLs.
    url_tf_->SetReadOnly(controller_->template_url()->prepopulate_id() != 0);
  } else {
    title_tf_ = CreateTextfield(std::wstring(), false);
    keyword_tf_ = CreateTextfield(std::wstring(), true);
    url_tf_ = CreateTextfield(std::wstring(), false);
  }
  title_iv_ = new ImageView();
  keyword_iv_ = new ImageView();
  url_iv_ = new ImageView();

  UpdateImageViews();

  const int related_x = kRelatedControlHorizontalSpacing;
  const int related_y = kRelatedControlVerticalSpacing;
  const int unrelated_y = kUnrelatedControlVerticalSpacing;

  // View and GridLayout take care of deleting GridLayout for us.
  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // Define the structure of the layout.

  // For the buttons.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(1, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes(1, 3, -1);

  // For the Textfields.
  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // For the description.
  column_set = layout->AddColumnSet(2);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Add the contents.
  layout->StartRow(0, 1);
  layout->AddView(CreateLabel(IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_LABEL));
  layout->AddView(title_tf_);
  layout->AddView(title_iv_);

  layout->StartRowWithPadding(0, 1, 0, related_y);
  layout->AddView(CreateLabel(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_LABEL));
  layout->AddView(keyword_tf_);
  layout->AddView(keyword_iv_);

  layout->StartRowWithPadding(0, 1, 0, related_y);
  layout->AddView(CreateLabel(IDS_SEARCH_ENGINES_EDITOR_URL_LABEL));
  layout->AddView(url_tf_);
  layout->AddView(url_iv_);

  // On RTL UIs (such as Arabic and Hebrew) the description text is not
  // displayed correctly since it contains the substring "%s". This substring
  // is not interpreted by the Unicode BiDi algorithm as an LTR string and
  // therefore the end result is that the following right to left text is
  // displayed: ".three two s% one" (where 'one', 'two', etc. are words in
  // Hebrew).
  //
  // In order to fix this problem we transform the substring "%s" so that it
  // is displayed correctly when rendered in an RTL context.
  layout->StartRowWithPadding(0, 2, 0, unrelated_y);
  std::wstring description =
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_URL_DESCRIPTION_LABEL);
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    const std::wstring reversed_percent(L"s%");
    std::wstring::size_type percent_index =
        description.find(L"%s", static_cast<std::wstring::size_type>(0));
    if (percent_index != std::wstring::npos)
      description.replace(percent_index,
                          reversed_percent.length(),
                          reversed_percent);
  }

  views::Label* description_label = new views::Label(description);
  description_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(description_label);

  layout->AddPaddingRow(0, related_y);
}

views::Label* EditSearchEngineDialog::CreateLabel(int message_id) {
  views::Label* label = new views::Label(l10n_util::GetString(message_id));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

Textfield* EditSearchEngineDialog::CreateTextfield(const std::wstring& text,
                                                   bool lowercase) {
  Textfield* text_field = new Textfield(
      lowercase ? Textfield::STYLE_LOWERCASE : Textfield::STYLE_DEFAULT);
  text_field->SetText(text);
  text_field->SetController(this);
  return text_field;
}

void EditSearchEngineDialog::UpdateImageViews() {
  UpdateImageView(keyword_iv_, controller_->IsKeywordValid(keyword_tf_->text()),
                  IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT);
  UpdateImageView(url_iv_, controller_->IsURLValid(url_tf_->text()),
                  IDS_SEARCH_ENGINES_INVALID_URL_TT);
  UpdateImageView(title_iv_, controller_->IsTitleValid(title_tf_->text()),
                  IDS_SEARCH_ENGINES_INVALID_TITLE_TT);
}

void EditSearchEngineDialog::UpdateImageView(ImageView* image_view,
                                             bool is_valid,
                                             int invalid_message_id) {
  if (is_valid) {
    image_view->SetTooltipText(std::wstring());
    image_view->SetImage(
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_INPUT_GOOD));
  } else {
    image_view->SetTooltipText(l10n_util::GetString(invalid_message_id));
    image_view->SetImage(
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_INPUT_ALERT));
  }
}
