// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_table_model.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/table_model_observer.h"
#include "base/gfx/png_decoder.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

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
      : template_url_(template_url),
        load_state_(NOT_LOADED),
        model_(model) {
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
        GURL url = GURL(WideToUTF16Hack(template_url_.url()->url()));
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
  template_url_model_->Load();
  template_url_model_->AddObserver(this);
  Reload();
}

TemplateURLTableModel::~TemplateURLTableModel() {
  template_url_model_->RemoveObserver(this);
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

void TemplateURLTableModel::SetObserver(TableModelObserver* observer) {
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
  // Remove the observer while we modify the model, that way we don't need to
  // worry about the model calling us back when we mutate it.
  template_url_model_->RemoveObserver(this);
  const TemplateURL* template_url = &GetTemplateURL(index);

  scoped_ptr<ModelEntry> entry(entries_[static_cast<int>(index)]);
  entries_.erase(entries_.begin() + static_cast<int>(index));
  if (index < last_search_engine_index_)
    last_search_engine_index_--;
  if (observer_)
    observer_->OnItemsRemoved(index, 1);

  // Make sure to remove from the table model first, otherwise the
  // TemplateURL would be freed.
  template_url_model_->Remove(template_url);
  template_url_model_->AddObserver(this);
}

void TemplateURLTableModel::Add(int index, TemplateURL* template_url) {
  DCHECK(index >= 0 && index <= RowCount());
  ModelEntry* entry = new ModelEntry(this, *template_url);
  entries_.insert(entries_.begin() + index, entry);
  if (observer_)
    observer_->OnItemsAdded(index, 1);
  template_url_model_->RemoveObserver(this);
  template_url_model_->Add(template_url);
  template_url_model_->AddObserver(this);
}

void TemplateURLTableModel::ModifyTemplateURL(int index,
                                              const std::wstring& title,
                                              const std::wstring& keyword,
                                              const std::wstring& url) {
  DCHECK(index >= 0 && index <= RowCount());
  const TemplateURL* template_url = &GetTemplateURL(index);
  template_url_model_->RemoveObserver(this);
  template_url_model_->ResetTemplateURL(template_url, title, keyword, url);
  if (template_url_model_->GetDefaultSearchProvider() == template_url &&
      !TemplateURL::SupportsReplacement(template_url)) {
    // The entry was the default search provider, but the url has been modified
    // so that it no longer supports replacement. Reset the default search
    // provider so that it doesn't point to a bogus entry.
    template_url_model_->SetDefaultSearchProvider(NULL);
  }
  template_url_model_->AddObserver(this);
  ReloadIcon(index);  // Also calls NotifyChanged().
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

int TemplateURLTableModel::MoveToMainGroup(int index) {
  if (index < last_search_engine_index_)
    return -1; // Already in the main group.

  ModelEntry* current_entry = entries_[index];
  entries_.erase(index + entries_.begin());
  if (observer_)
    observer_->OnItemsRemoved(index, 1);

  const int new_index = last_search_engine_index_++;
  entries_.insert(entries_.begin() + new_index, current_entry);
  if (observer_)
    observer_->OnItemsAdded(new_index, 1);
  return new_index;
}

int TemplateURLTableModel::MakeDefaultTemplateURL(int index) {
  if (index < 0 || index >= RowCount()) {
    NOTREACHED();
    return -1;
  }

  const TemplateURL* keyword = &GetTemplateURL(index);
  const TemplateURL* current_default =
      template_url_model_->GetDefaultSearchProvider();
  if (current_default == keyword)
    return -1;

  template_url_model_->RemoveObserver(this);
  template_url_model_->SetDefaultSearchProvider(keyword);
  template_url_model_->AddObserver(this);

  // The formatting of the default engine is different; notify the table that
  // both old and new entries have changed.
  if (current_default != NULL)
    NotifyChanged(IndexOfTemplateURL(current_default));
  const int new_index = IndexOfTemplateURL(keyword);
  NotifyChanged(new_index);

  // Make sure the new default is in the main group.
  return MoveToMainGroup(index);
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

void TemplateURLTableModel::OnTemplateURLModelChanged() {
  Reload();
}
