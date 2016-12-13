// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/keyword_editor_view.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/grid_layout.h"
#include "views/controls/button/native_button.h"
#include "views/controls/table/table_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

using views::GridLayout;
using views::NativeButton;

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowKeywordEditorView(Profile* profile) {
  KeywordEditorView::Show(profile);
}

}  // namespace browser


// KeywordEditorView ----------------------------------------------------------

// If non-null, there is an open editor and this is the window it is contained
// in.
static views::Window* open_window = NULL;

// static
void KeywordEditorView::Show(Profile* profile) {
  if (!profile->GetTemplateURLModel())
    return;

  if (open_window != NULL)
    open_window->Close();
  DCHECK(!open_window);

  // Both of these will be deleted when the dialog closes.
  KeywordEditorView* keyword_editor = new KeywordEditorView(profile);

  // Initialize the UI. By passing in an empty rect KeywordEditorView is
  // queried for its preferred size.
  open_window = views::Window::CreateChromeWindow(NULL, gfx::Rect(),
                                                  keyword_editor);

  open_window->Show();
}

KeywordEditorView::KeywordEditorView(Profile* profile)
    : profile_(profile),
      controller_(new KeywordEditorController(profile)) {
  DCHECK(controller_->url_model());
  controller_->url_model()->AddObserver(this);
  Init();
}

KeywordEditorView::~KeywordEditorView() {
  table_view_->SetModel(NULL);
  controller_->url_model()->RemoveObserver(this);
}

void KeywordEditorView::OnEditedKeyword(const TemplateURL* template_url,
                                        const std::wstring& title,
                                        const std::wstring& keyword,
                                        const std::wstring& url) {
  if (template_url) {
    controller_->ModifyTemplateURL(template_url, title, keyword, url);

    // Force the make default button to update.
    OnSelectionChanged();
  } else {
    table_view_->Select(controller_->AddTemplateURL(title, keyword, url));
  }
}


gfx::Size KeywordEditorView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_SEARCHENGINES_DIALOG_WIDTH_CHARS,
      IDS_SEARCHENGINES_DIALOG_HEIGHT_LINES));
}

bool KeywordEditorView::CanResize() const {
  return true;
}

std::wstring KeywordEditorView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_WINDOW_TITLE);
}

int KeywordEditorView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool KeywordEditorView::Accept() {
  open_window = NULL;
  return true;
}

bool KeywordEditorView::Cancel() {
  open_window = NULL;
  return true;
}

views::View* KeywordEditorView::GetContentsView() {
  return this;
}

void KeywordEditorView::Init() {
  std::vector<TableColumn> columns;
  columns.push_back(
      TableColumn(IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN,
                  TableColumn::LEFT, -1, .75));
  columns.back().sortable = true;
  columns.push_back(
      TableColumn(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN,
                  TableColumn::LEFT, -1, .25));
  columns.back().sortable = true;
  table_view_ = new views::TableView(controller_->table_model(), columns,
      views::ICON_AND_TEXT, false, true, true);
  table_view_->SetObserver(this);

  add_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_NEW_BUTTON));
  add_button_->SetEnabled(controller_->loaded());

  edit_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_EDIT_BUTTON));
  edit_button_->SetEnabled(false);

  remove_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_REMOVE_BUTTON));
  remove_button_->SetEnabled(false);

  make_default_button_ = new views::NativeButton(
      this,
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_MAKE_DEFAULT_BUTTON));
  make_default_button_->SetEnabled(false);

  InitLayoutManager();
}

void KeywordEditorView::InitLayoutManager() {
  const int related_x = kRelatedControlHorizontalSpacing;
  const int related_y = kRelatedControlVerticalSpacing;
  const int unrelated_y = kUnrelatedControlVerticalSpacing;

  GridLayout* contents_layout = CreatePanelGridLayout(this);
  SetLayoutManager(contents_layout);

  // For the table and buttons.
  views::ColumnSet* column_set = contents_layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  contents_layout->StartRow(0, 0);
  contents_layout->AddView(table_view_, 1, 8, GridLayout::FILL,
                           GridLayout::FILL);
  contents_layout->AddView(add_button_);

  contents_layout->StartRowWithPadding(0, 0, 0, related_y);
  contents_layout->SkipColumns(2);
  contents_layout->AddView(edit_button_);

  contents_layout->StartRowWithPadding(0, 0, 0, related_y);
  contents_layout->SkipColumns(2);
  contents_layout->AddView(remove_button_);

  contents_layout->StartRowWithPadding(0, 0, 0, related_y);
  contents_layout->SkipColumns(2);
  contents_layout->AddView(make_default_button_);

  contents_layout->AddPaddingRow(1, 0);
}

void KeywordEditorView::OnSelectionChanged() {
  const int selected_row_count = table_view_->SelectedRowCount();
  edit_button_->SetEnabled(selected_row_count == 1);
  bool can_make_default = false;
  bool can_remove = false;
  if (selected_row_count == 1) {
    const TemplateURL* selected_url =
        controller_->GetTemplateURL(table_view_->FirstSelectedRow());
    can_make_default = controller_->CanMakeDefault(selected_url);
    can_remove = controller_->CanRemove(selected_url);
  }
  remove_button_->SetEnabled(can_remove);
  make_default_button_->SetEnabled(can_make_default);
}

void KeywordEditorView::OnDoubleClick() {
  if (edit_button_->IsEnabled())
    ButtonPressed(edit_button_);
}

void KeywordEditorView::ButtonPressed(views::Button* sender) {
  if (sender == add_button_) {
    browser::EditSearchEngine(GetWindow()->GetNativeWindow(), NULL, this,
                              profile_);
  } else if (sender == remove_button_) {
    DCHECK(table_view_->SelectedRowCount() == 1);
    int last_view_row = -1;
    for (views::TableView::iterator i = table_view_->SelectionBegin();
         i != table_view_->SelectionEnd(); ++i) {
      last_view_row = table_view_->model_to_view(*i);
      controller_->RemoveTemplateURL(*i);
    }
    if (last_view_row >= controller_->table_model()->RowCount())
      last_view_row = controller_->table_model()->RowCount() - 1;
    if (last_view_row >= 0)
      table_view_->Select(table_view_->view_to_model(last_view_row));
  } else if (sender == edit_button_) {
    const int selected_row = table_view_->FirstSelectedRow();
    const TemplateURL* template_url =
        controller_->GetTemplateURL(selected_row);
    browser::EditSearchEngine(GetWindow()->GetNativeWindow(), template_url,
                              this, profile_);
  } else if (sender == make_default_button_) {
    MakeDefaultTemplateURL();
  } else {
    NOTREACHED();
  }
}

void KeywordEditorView::OnTemplateURLModelChanged() {
  add_button_->SetEnabled(controller_->loaded());
}

void KeywordEditorView::MakeDefaultTemplateURL() {
  int new_index =
      controller_->MakeDefaultTemplateURL(table_view_->FirstSelectedRow());
  if (new_index >= 0)
    table_view_->Select(new_index);
}
