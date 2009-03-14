// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/keyword_editor_view.h"

#include <vector>

#include "base/gfx/png_decoder.h"
#include "base/string_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/edit_keyword_controller.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/views/background.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/text_field.h"
#include "chrome/views/widget.h"
#include "chrome/views/window.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "skia/include/SkBitmap.h"

using views::GridLayout;
using views::NativeButton;
using views::TableColumn;

// Group IDs used by TemplateURLTableModel.
static const int kMainGroupID = 0;
static const int kOtherGroupID = 1;

// ModelEntry ----------------------------------------------------

// ModelEntry wraps a TemplateURL as returned from the TemplateURL.
// ModelEntry also tracks state information about the URL.

// Icon used while loading, or if a specific favicon can't be found.
static SkBitmap* default_icon = NULL;

class ModelEntry {
 public:
  explicit ModelEntry(TemplateURLTableModel* model,
                      const TemplateURL& template_url)
      : model_(model),
        template_url_(template_url) {
    load_state_ = NOT_LOADED;
    if (!default_icon) {
      default_icon = ResourceBundle::GetSharedInstance().
          GetBitmapNamed(IDR_DEFAULT_FAVICON);
    }
  }

  const TemplateURL& template_url() {
    return template_url_;
  }

  SkBitmap GetIcon() {
    if (load_state_ == NOT_LOADED)
      LoadFavIcon();
    if (!fav_icon_.isNull())
      return fav_icon_;
    return *default_icon;
  }

  // Resets internal status so that the next time the icon is asked for its
  // fetched again. This should be invoked if the url is modified.
  void ResetIcon() {
    load_state_ = NOT_LOADED;
    fav_icon_ = SkBitmap();
  }

 private:
  // State of the favicon.
  enum LoadState {
    NOT_LOADED,
    LOADING,
    LOADED
  };

  void LoadFavIcon() {
    load_state_ = LOADED;
    HistoryService* hs =
        model_->template_url_model()->profile()->GetHistoryService(
            Profile::EXPLICIT_ACCESS);
    if (!hs)
      return;
    GURL fav_icon_url = template_url().GetFavIconURL();
    if (!fav_icon_url.is_valid()) {
      // The favicon url isn't always set. Guess at one here.
      if (template_url_.url() && template_url_.url()->IsValid()) {
        GURL url = GURL(template_url_.url()->url());
        if (url.is_valid())
          fav_icon_url = TemplateURL::GenerateFaviconURL(url);
      }
      if (!fav_icon_url.is_valid())
        return;
    }
    load_state_ = LOADING;
    hs->GetFavIcon(fav_icon_url,
                   &request_consumer_,
                   NewCallback(this, &ModelEntry::OnFavIconDataAvailable));
  }

  void OnFavIconDataAvailable(
      HistoryService::Handle handle,
      bool know_favicon,
      scoped_refptr<RefCountedBytes> data,
      bool expired,
      GURL icon_url) {
    load_state_ = LOADED;
    if (know_favicon && data.get() &&
        PNGDecoder::Decode(&data->data, &fav_icon_)) {
      model_->FavIconAvailable(this);
    }
  }

  const TemplateURL& template_url_;
  SkBitmap fav_icon_;
  LoadState load_state_;
  TemplateURLTableModel* model_;
  CancelableRequestConsumer request_consumer_;

  DISALLOW_EVIL_CONSTRUCTORS(ModelEntry);
};

// TemplateURLTableModel -----------------------------------------

TemplateURLTableModel::TemplateURLTableModel(
    TemplateURLModel* template_url_model)
    : observer_(NULL),
      template_url_model_(template_url_model) {
  DCHECK(template_url_model);
  Reload();
}

TemplateURLTableModel::~TemplateURLTableModel() {
  STLDeleteElements(&entries_);
  entries_.clear();
}

void TemplateURLTableModel::Reload() {
  STLDeleteElements(&entries_);
  entries_.clear();

  std::vector<const TemplateURL*> urls = template_url_model_->GetTemplateURLs();

  // Keywords that can be made the default first.
  for (std::vector<const TemplateURL*>::iterator i = urls.begin();
       i != urls.end(); ++i) {
    const TemplateURL& template_url = *(*i);
    // NOTE: we don't use ShowInDefaultList here to avoid items bouncing around
    // the lists while editing.
    if (template_url.show_in_default_list())
      entries_.push_back(new ModelEntry(this, template_url));
  }

  last_search_engine_index_ = static_cast<int>(entries_.size());

  // Then the rest.
  for (std::vector<const TemplateURL*>::iterator i = urls.begin();
       i != urls.end(); ++i) {
    const TemplateURL* template_url = *i;
    // NOTE: we don't use ShowInDefaultList here to avoid things bouncing
    // the lists while editing.
    if (!template_url->show_in_default_list())
      entries_.push_back(new ModelEntry(this, *template_url));
  }

  if (observer_)
    observer_->OnModelChanged();
}

int TemplateURLTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

std::wstring TemplateURLTableModel::GetText(int row, int col_id) {
  DCHECK(row >= 0 && row < RowCount());
  const TemplateURL& url = entries_[row]->template_url();

  switch (col_id) {
    case IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN: {
      std::wstring url_short_name = url.short_name();
      // TODO(xji): Consider adding a special case if the short name is a URL,
      // since those should always be displayed LTR. Please refer to
      // http://crbug.com/6726 for more information.
      l10n_util::AdjustStringForLocaleDirection(url_short_name,
                                                &url_short_name);
      return (template_url_model_->GetDefaultSearchProvider() == &url) ?
          l10n_util::GetStringF(IDS_SEARCH_ENGINES_EDITOR_DEFAULT_ENGINE,
                                url_short_name) : url_short_name;
    }

    case IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN: {
      const std::wstring& keyword = url.keyword();
      // Keyword should be domain name. Force it to have LTR directionality.
      if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
        std::wstring localized_keyword = keyword;
        l10n_util::WrapStringWithLTRFormatting(&localized_keyword);
        return localized_keyword;
      }
      return keyword;
      break;
    }

    default:
      NOTREACHED();
      return std::wstring();
  }
}

SkBitmap TemplateURLTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return entries_[row]->GetIcon();
}

void TemplateURLTableModel::SetObserver(views::TableModelObserver* observer) {
  observer_ = observer;
}

bool TemplateURLTableModel::HasGroups() {
  return true;
}

TemplateURLTableModel::Groups TemplateURLTableModel::GetGroups() {
  Groups groups;

  Group search_engine_group;
  search_engine_group.title =
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_MAIN_SEPARATOR);
  search_engine_group.id = kMainGroupID;
  groups.push_back(search_engine_group);

  Group other_group;
  other_group.title =
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_OTHER_SEPARATOR);
  other_group.id = kOtherGroupID;
  groups.push_back(other_group);

  return groups;
}

int TemplateURLTableModel::GetGroupID(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return row < last_search_engine_index_ ? kMainGroupID : kOtherGroupID;
}

void TemplateURLTableModel::Remove(int index) {
  scoped_ptr<ModelEntry> entry(entries_[static_cast<int>(index)]);
  entries_.erase(entries_.begin() + static_cast<int>(index));
  if (index < last_search_engine_index_)
    last_search_engine_index_--;
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

void TemplateURLTableModel::Add(int index, const TemplateURL* template_url) {
  DCHECK(index >= 0 && index <= RowCount());
  ModelEntry* entry = new ModelEntry(this, *template_url);
  entries_.insert(entries_.begin() + index, entry);
  if (observer_)
    observer_->OnItemsAdded(index, 1);
}

void TemplateURLTableModel::ReloadIcon(int index) {
  DCHECK(index >= 0 && index < RowCount());

  entries_[index]->ResetIcon();

  NotifyChanged(index);
}

const TemplateURL& TemplateURLTableModel::GetTemplateURL(int index) {
  return entries_[index]->template_url();
}

int TemplateURLTableModel::IndexOfTemplateURL(
    const TemplateURL* template_url) {
  for (std::vector<ModelEntry*>::iterator i = entries_.begin();
       i != entries_.end(); ++i) {
    ModelEntry* entry = *i;
    if (&(entry->template_url()) == template_url)
      return static_cast<int>(i - entries_.begin());
  }
  return -1;
}

void TemplateURLTableModel::MoveToMainGroup(int index) {
  if (index < last_search_engine_index_)
    return; // Already in the main group.

  ModelEntry* current_entry = entries_[index];
  entries_.erase(index + entries_.begin());
  if (observer_)
    observer_->OnItemsRemoved(index, 1);

  const int new_index = last_search_engine_index_++;
  entries_.insert(entries_.begin() + new_index, current_entry);
  if (observer_)
    observer_->OnItemsAdded(new_index, 1);
}

void TemplateURLTableModel::NotifyChanged(int index) {
  if (observer_)
    observer_->OnItemsChanged(index, 1);
}

void TemplateURLTableModel::FavIconAvailable(ModelEntry* entry) {
  std::vector<ModelEntry*>::iterator i =
      find(entries_.begin(), entries_.end(), entry);
  DCHECK(i != entries_.end());
  NotifyChanged(static_cast<int>(i - entries_.begin()));
}

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
      url_model_(profile->GetTemplateURLModel()) {
  DCHECK(url_model_);
  Init();
}

KeywordEditorView::~KeywordEditorView() {
  // Only remove the listener if we installed one.
  if (table_model_.get()) {
    table_view_->SetModel(NULL);
    url_model_->RemoveObserver(this);
  }
}

void KeywordEditorView::AddTemplateURL(const std::wstring& title,
                                       const std::wstring& keyword,
                                       const std::wstring& url) {
  DCHECK(!url.empty());

  UserMetrics::RecordAction(L"KeywordEditor_AddKeyword", profile_);

  TemplateURL* template_url = new TemplateURL();
  template_url->set_short_name(title);
  template_url->set_keyword(keyword);
  template_url->SetURL(url, 0, 0);

  // There's a bug (1090726) in TableView with groups enabled such that newly
  // added items in groups ALWAYS appear at the end, regardless of the index
  // passed in. Worse yet, the selected rows get messed up when this happens
  // causing other problems. As a work around we always add the item to the end
  // of the list.
  const int new_index = table_model_->RowCount();
  url_model_->RemoveObserver(this);
  table_model_->Add(new_index, template_url);
  url_model_->Add(template_url);
  url_model_->AddObserver(this);

  table_view_->Select(new_index);
}

void KeywordEditorView::ModifyTemplateURL(const TemplateURL* template_url,
                                          const std::wstring& title,
                                          const std::wstring& keyword,
                                          const std::wstring& url) {
  const int index = table_model_->IndexOfTemplateURL(template_url);
  if (index == -1) {
    // Will happen if url was deleted out from under us while the user was
    // editing it.
    return;
  }

  // Don't do anything if the entry didn't change.
  if (template_url->short_name() == title &&
      template_url->keyword() == keyword &&
      ((url.empty() && !template_url->url()) ||
       (!url.empty() && template_url->url() &&
        template_url->url()->url() == url))) {
    return;
  }

  url_model_->RemoveObserver(this);
  url_model_->ResetTemplateURL(template_url, title, keyword, url);
  if (url_model_->GetDefaultSearchProvider() == template_url &&
      (!template_url->url() || !template_url->url()->SupportsReplacement())) {
    // The entry was the default search provider, but the url has been modified
    // so that it no longer supports replacement. Reset the default search
    // provider so that it doesn't point to a bogus entry.
    url_model_->SetDefaultSearchProvider(NULL);
  }
  url_model_->AddObserver(this);
  table_model_->ReloadIcon(index);  // Also calls NotifyChanged().

  // Force the make default button to update.
  OnSelectionChanged();

  UserMetrics::RecordAction(L"KeywordEditor_ModifiedKeyword", profile_);
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
  return DIALOGBUTTON_CANCEL;
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
  DCHECK(!table_model_.get());

  url_model_->Load();
  url_model_->AddObserver(this);

  table_model_.reset(new TemplateURLTableModel(url_model_));

  std::vector<TableColumn> columns;
  columns.push_back(
      TableColumn(IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN,
                  TableColumn::LEFT, -1, .75));
  columns.back().sortable = true;
  columns.push_back(
      TableColumn(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN,
                  TableColumn::LEFT, -1, .25));
  columns.back().sortable = true;
  table_view_ = new views::TableView(table_model_.get(), columns,
      views::ICON_AND_TEXT, false, true, true);
  table_view_->SetObserver(this);

  add_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_NEW_BUTTON));
  add_button_->SetEnabled(url_model_->loaded());
  add_button_->SetListener(this);

  edit_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_EDIT_BUTTON));
  edit_button_->SetEnabled(false);
  edit_button_->SetListener(this);

  remove_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_REMOVE_BUTTON));
  remove_button_->SetEnabled(false);
  remove_button_->SetListener(this);

  make_default_button_ = new views::NativeButton(
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_MAKE_DEFAULT_BUTTON));
  make_default_button_->SetEnabled(false);
  make_default_button_->SetListener(this);

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
        &table_model_->GetTemplateURL(table_view_->FirstSelectedRow());
    can_make_default =
        (selected_url != url_model_->GetDefaultSearchProvider() &&
         selected_url->url() &&
         selected_url->url()->SupportsReplacement());
    can_remove = (selected_url != url_model_->GetDefaultSearchProvider());
  }
  remove_button_->SetEnabled(can_remove);
  make_default_button_->SetEnabled(can_make_default);
}

void KeywordEditorView::OnDoubleClick() {
  if (edit_button_->IsEnabled())
    ButtonPressed(edit_button_);
}

void KeywordEditorView::ButtonPressed(views::NativeButton* sender) {
  if (sender == add_button_) {
    EditKeywordController* controller = 
        new EditKeywordController(GetWidget()->GetNativeView(), NULL, this,
                                  profile_);
    controller->Show();
  } else if (sender == remove_button_) {
    DCHECK(table_view_->SelectedRowCount() > 0);
    // Remove the observer while we modify the model, that way we don't need to
    // worry about the model calling us back when we mutate it.
    url_model_->RemoveObserver(this);
    int last_view_row = -1;
    for (views::TableView::iterator i = table_view_->SelectionBegin();
         i != table_view_->SelectionEnd(); ++i) {
      last_view_row = table_view_->model_to_view(*i);
      const TemplateURL* template_url = &table_model_->GetTemplateURL(*i);
      // Make sure to remove from the table model first, otherwise the
      // TemplateURL would be freed.
      table_model_->Remove(*i);
      url_model_->Remove(template_url);
    }
    if (last_view_row >= table_model_->RowCount())
      last_view_row = table_model_->RowCount() - 1;
    if (last_view_row >= 0)
      table_view_->Select(table_view_->view_to_model(last_view_row));
    url_model_->AddObserver(this);
    UserMetrics::RecordAction(L"KeywordEditor_RemoveKeyword", profile_);
  } else if (sender == edit_button_) {
    const int selected_row = table_view_->FirstSelectedRow();
    const TemplateURL* template_url =
        &table_model_->GetTemplateURL(selected_row);
    EditKeywordController* controller =
        new EditKeywordController(GetWidget()->GetNativeView(), template_url,
                                  this, profile_);
    controller->Show();
  } else if (sender == make_default_button_) {
    MakeDefaultSearchProvider();
  } else {
    NOTREACHED();
  }
}

void KeywordEditorView::OnTemplateURLModelChanged() {
  table_model_->Reload();
  add_button_->SetEnabled(url_model_->loaded());
}

void KeywordEditorView::MakeDefaultSearchProvider() {
  MakeDefaultSearchProvider(table_view_->FirstSelectedRow());
}

void KeywordEditorView::MakeDefaultSearchProvider(int index) {
  if (index < 0 || index >= table_model_->RowCount()) {
    NOTREACHED();
    return;
  }
  const TemplateURL* keyword = &table_model_->GetTemplateURL(index);
  const TemplateURL* current_default = url_model_->GetDefaultSearchProvider();
  if (current_default == keyword)
    return;

  url_model_->RemoveObserver(this);
  url_model_->SetDefaultSearchProvider(keyword);
  url_model_->AddObserver(this);

  // The formatting of the default engine is different; notify the table that
  // both old and new entries have changed.
  if (current_default != NULL) {
    table_model_->NotifyChanged(table_model_->IndexOfTemplateURL(
        current_default));
  }
  const int new_index = table_model_->IndexOfTemplateURL(keyword);
  table_model_->NotifyChanged(new_index);

  // Make sure the new default is in the main group.
  table_model_->MoveToMainGroup(index);

  // And select it.
  table_view_->Select(table_model_->IndexOfTemplateURL(keyword));
}
