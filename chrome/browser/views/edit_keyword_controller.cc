// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/edit_keyword_controller.h"

#include "base/string_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/label.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/image_view.h"
#include "chrome/views/table_view.h"
#include "chrome/views/window.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

using views::GridLayout;
using views::ImageView;
using views::TextField;


namespace {
// Converts a URL as understood by TemplateURL to one appropriate for display
// to the user.
std::wstring GetDisplayURL(const TemplateURL& turl) {
  return turl.url() ? turl.url()->DisplayURL() : std::wstring();
}
}  // namespace

EditKeywordController::EditKeywordController(
    HWND parent,
    const TemplateURL* template_url,
    KeywordEditorView* keyword_editor_view,
    Profile* profile)
    : parent_(parent),
      template_url_(template_url),
      keyword_editor_view_(keyword_editor_view),
      profile_(profile) {
  DCHECK(profile_);
  Init();
}

void EditKeywordController::Show() {
  // Window interprets an empty rectangle as needing to query the content for
  // the size as well as centering relative to the parent.
  views::Window::CreateChromeWindow(::IsWindow(parent_) ? parent_ : NULL,
                                    gfx::Rect(), this); 
  window()->Show();
  GetDialogClientView()->UpdateDialogButtons();
  title_tf_->SelectAll();
  title_tf_->RequestFocus();
}

bool EditKeywordController::IsModal() const {
  // If we were called without a KeywordEditorView, and our associated
  // window happens to have gone away while the TemplateURLFetcher was
  // loading, we might not have a valid parent anymore.
  // ::IsWindow() returns a BOOL, which is a typedef for an int and causes a
  // warning if we try to return it or cast it as a bool.
  if (::IsWindow(parent_))
    return true;
  return false;
}

std::wstring EditKeywordController::GetWindowTitle() const {
  return l10n_util::GetString(template_url_ ?
      IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE :
      IDS_SEARCH_ENGINES_EDITOR_NEW_WINDOW_TITLE);
}

int EditKeywordController::GetDialogButtons() const {
  return DIALOGBUTTON_OK | DIALOGBUTTON_CANCEL;
}

bool EditKeywordController::IsDialogButtonEnabled(DialogButton button) const {
  if (button == DIALOGBUTTON_OK) {
    return (IsKeywordValid() && !title_tf_->GetText().empty() && IsURLValid());
  }
  return true;
}

void EditKeywordController::WindowClosing() {
  // User canceled the save, delete us.
  delete this;
}

bool EditKeywordController::Cancel() {
  CleanUpCancelledAdd();
  return true;
}

bool EditKeywordController::Accept() {
  std::wstring url_string = GetURL();
  DCHECK(!url_string.empty());
  const std::wstring& keyword = keyword_tf_->GetText();

  const TemplateURL* existing =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  if (existing &&
      (!keyword_editor_view_ || existing != template_url_)) {
    // An entry may have been added with the same keyword string while the
    // user edited the dialog, either automatically or by the user (if we're
    // confirming a JS addition, they could have the Options dialog open at the
    // same time). If so, just ignore this add.
    // TODO(pamg): Really, we should modify the entry so this later one
    // overwrites it. But we don't expect this case to be common.
    CleanUpCancelledAdd();
    return true;
  }

  if (!keyword_editor_view_) {
    // Confiming an entry we got from JS. We have a template_url_, but it
    // hasn't yet been added to the model.
    DCHECK(template_url_);
    // const_cast is ugly, but this is the same thing the TemplateURLModel
    // does in a similar situation (updating an existing TemplateURL with
    // data from a new one).
    TemplateURL* modifiable_url = const_cast<TemplateURL*>(template_url_);
    modifiable_url->set_short_name(title_tf_->GetText());
    modifiable_url->set_keyword(keyword);
    modifiable_url->SetURL(url_string, 0, 0);
    // TemplateURLModel takes ownership of template_url_.
    profile_->GetTemplateURLModel()->Add(modifiable_url);
    UserMetrics::RecordAction(L"KeywordEditor_AddKeywordJS", profile_);
  } else if (!template_url_) {
    // Adding a new entry via the KeywordEditorView.
    DCHECK(keyword_editor_view_);
    if (keyword_editor_view_)
      keyword_editor_view_->AddTemplateURL(title_tf_->GetText(),
                                           keyword_tf_->GetText(),
                                           url_string);
  } else {
    // Modifying an entry via the KeywordEditorView.
    DCHECK(keyword_editor_view_);
    if (keyword_editor_view_) {
      keyword_editor_view_->ModifyTemplateURL(template_url_,
                                              title_tf_->GetText(),
                                              keyword_tf_->GetText(),
                                              url_string);
    }
  }
  return true;
}

views::View* EditKeywordController::GetContentsView() {
  return view_;
}

void EditKeywordController::ContentsChanged(TextField* sender,
                                            const std::wstring& new_contents) {
  GetDialogClientView()->UpdateDialogButtons();
  UpdateImageViews();
}

void EditKeywordController::HandleKeystroke(TextField* sender,
                                            UINT message,
                                            TCHAR key,
                                            UINT repeat_count,
                                            UINT flags) {
}

void EditKeywordController::Init() {
  // Create the views we'll need.
  view_ = new views::View();
  if (template_url_) {
    title_tf_ = CreateTextField(template_url_->short_name(), false);
    keyword_tf_ = CreateTextField(template_url_->keyword(), true);
    url_tf_ = CreateTextField(GetDisplayURL(*template_url_), false);
    // We don't allow users to edit prepopulate URLs. This is done as
    // occasionally we need to update the URL of prepopulated TemplateURLs.
    url_tf_->SetReadOnly(template_url_->prepopulate_id() != 0);
  } else {
    title_tf_ = CreateTextField(std::wstring(), false);
    keyword_tf_ = CreateTextField(std::wstring(), true);
    url_tf_ = CreateTextField(std::wstring(), false);
  }
  title_iv_ = new ImageView();
  keyword_iv_ = new ImageView();
  url_iv_ = new ImageView();

  UpdateImageViews();

  const int related_x = kRelatedControlHorizontalSpacing;
  const int related_y = kRelatedControlVerticalSpacing;
  const int unrelated_y = kUnrelatedControlVerticalSpacing;

  // View and GridLayout take care of deleting GridLayout for us.
  GridLayout* layout = CreatePanelGridLayout(view_);
  view_->SetLayoutManager(layout);

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

  // For the textfields.
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

views::Label* EditKeywordController::CreateLabel(int message_id) {
  views::Label* label = new views::Label(l10n_util::GetString(message_id));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

TextField* EditKeywordController::CreateTextField(const std::wstring& text,
                                                  bool lowercase) {
  TextField* text_field = new TextField(
      lowercase ? TextField::STYLE_LOWERCASE : TextField::STYLE_DEFAULT);
  text_field->SetText(text);
  text_field->SetController(this);
  return text_field;
}

bool EditKeywordController::IsURLValid() const {
  std::wstring url = GetURL();
  if (url.empty())
    return false;

  // Use TemplateURLRef to extract the search placeholder.
  TemplateURLRef template_ref(url, 0, 0);
  if (!template_ref.IsValid())
    return false;

  if (!template_ref.SupportsReplacement())
    return GURL(url).is_valid();

  // If the url has a search term, replace it with a random string and make
  // sure the resulting URL is valid. We don't check the validity of the url
  // with the search term as that is not necessarily valid.
  return template_ref.ReplaceSearchTerms(TemplateURL(), L"a",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()).is_valid();
}

std::wstring EditKeywordController::GetURL() const {
  std::wstring url;
  TrimWhitespace(TemplateURLRef::DisplayURLToURLRef(url_tf_->GetText()),
                 TRIM_ALL, &url);
  if (url.empty())
    return url;

  // Parse the string as a URL to determine the scheme. If a scheme hasn't been
  // specified, we add it.
  url_parse::Parsed parts;
  std::wstring scheme(URLFixerUpper::SegmentURL(url, &parts));
  if (!parts.scheme.is_valid()) {
    std::wstring fixed_scheme(scheme);
    fixed_scheme.append(L"://");
    url.insert(0, fixed_scheme);
  }

  return url;
}

bool EditKeywordController::IsKeywordValid() const {
  std::wstring keyword = keyword_tf_->GetText();
  if (keyword.empty())
    return true; // Always allow no keyword.
  const TemplateURL* turl_with_keyword =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  return (turl_with_keyword == NULL || turl_with_keyword == template_url_);
}

void EditKeywordController::UpdateImageViews() {
  UpdateImageView(keyword_iv_, IsKeywordValid(),
                  IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT);
  UpdateImageView(url_iv_, IsURLValid(), IDS_SEARCH_ENGINES_INVALID_URL_TT);
  UpdateImageView(title_iv_, !title_tf_->GetText().empty(),
                  IDS_SEARCH_ENGINES_INVALID_TITLE_TT);
}

void EditKeywordController::UpdateImageView(ImageView* image_view,
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

void EditKeywordController::CleanUpCancelledAdd() {
  if (!keyword_editor_view_ && template_url_) {
    // When we have no KeywordEditorView, we know that the template_url_
    // hasn't yet been added to the model, so we need to clean it up.
    delete template_url_;
    template_url_ = NULL;
  }
}

