// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/possible_url_model.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/table_model_observer.h"
#include "base/gfx/png_decoder.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"

using base::Time;
using base::TimeDelta;

namespace {

// The default favicon.
SkBitmap* default_fav_icon = NULL;

// How long we query entry points for.
const int kPossibleURLTimeScope = 30;

}  // anonymous namespace

PossibleURLModel::PossibleURLModel() : profile_(NULL) {
  if (!default_fav_icon) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_fav_icon = rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
  }
}

void PossibleURLModel::Reload(Profile *profile) {
  profile_ = profile;
  consumer_.CancelAllRequests();
  HistoryService* hs =
      profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    history::QueryOptions options;
    options.end_time = Time::Now();
    options.begin_time =
        options.end_time - TimeDelta::FromDays(kPossibleURLTimeScope);
    options.most_recent_visit_only = true;
    options.max_count = 50;

    hs->QueryHistory(std::wstring(), options, &consumer_,
        NewCallback(this, &PossibleURLModel::OnHistoryQueryComplete));
  }
}

void PossibleURLModel::OnHistoryQueryComplete(HistoryService::Handle h,
                                              history::QueryResults* result) {
  results_.resize(result->size());
  std::wstring languages = profile_
      ? profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)
      : std::wstring();
  for (size_t i = 0; i < result->size(); ++i) {
    results_[i].url = (*result)[i].url();
    results_[i].index = i;
    results_[i].display_url =
        gfx::SortedDisplayURL((*result)[i].url(), languages);
    results_[i].title = (*result)[i].title();
  }

  // The old version of this code would filter out all but the most recent
  // visit to each host, plus all typed URLs and AUTO_BOOKMARK transitions. I
  // think this dialog has a lot of work, and I'm not sure those old
  // conditions are correct (the results look about equal quality for my
  // history with and without those conditions), so I'm not spending time
  // re-implementing them here. They used to be implemented in the history
  // service, but I think they should be implemented here because that was
  // pretty specific behavior that shouldn't be generally exposed.

  fav_icon_map_.clear();
  if (observer_)
    observer_->OnModelChanged();
}

const GURL& PossibleURLModel::GetURL(int row) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return GURL::EmptyGURL();
  }
  return results_[row].url;
}

const std::wstring& PossibleURLModel::GetTitle(int row) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return EmptyWString();
  }
  return results_[row].title;
}

std::wstring PossibleURLModel::GetText(int row, int col_id) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return std::wstring();
  }

  if (col_id == IDS_ASI_PAGE_COLUMN) {
    const std::wstring& title = GetTitle(row);
    // TODO(xji): Consider adding a special case if the title text is a URL,
    // since those should always have LTR directionality. Please refer to
    // http://crbug.com/6726 for more information.
    std::wstring localized_title;
    if (l10n_util::AdjustStringForLocaleDirection(title, &localized_title))
      return localized_title;
    return title;
  }

  // TODO(brettw): this should probably pass the GURL up so the URL elider
  // can be used at a higher level when we know the width.
  const string16& url = results_[row].display_url.display_url();
  if (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT)
    return UTF16ToWideHack(url);
  // Force URL to be LTR.
  std::wstring localized_url = UTF16ToWideHack(url);
  l10n_util::WrapStringWithLTRFormatting(&localized_url);
  return localized_url;
}

SkBitmap PossibleURLModel::GetIcon(int row) {
  if (row < 0 || row >= RowCount()) {
    NOTREACHED();
    return *default_fav_icon;
  }

  Result& result = results_[row];
  FavIconMap::iterator i = fav_icon_map_.find(result.index);
  if (i != fav_icon_map_.end()) {
    // We already requested the favicon, return it.
    if (!i->second.isNull())
      return i->second;
  } else if (profile_) {
    HistoryService* hs =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs) {
      CancelableRequestProvider::Handle h =
          hs->GetFavIconForURL(
              result.url, &consumer_,
              NewCallback(this, &PossibleURLModel::OnFavIconAvailable));
      consumer_.SetClientData(hs, h, result.index);
      // Add an entry to the map so that we don't attempt to request the
      // favicon again.
      fav_icon_map_[result.index] = SkBitmap();
    }
  }
  return *default_fav_icon;
}

int PossibleURLModel::CompareValues(int row1, int row2, int column_id) {
  if (column_id == IDS_ASI_URL_COLUMN) {
    return results_[row1].display_url.Compare(
        results_[row2].display_url, GetCollator());
  }
  return TableModel::CompareValues(row1, row2, column_id);
}

void PossibleURLModel::OnFavIconAvailable(
    HistoryService::Handle h,
    bool fav_icon_available,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  if (profile_) {
    HistoryService* hs =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    size_t index = consumer_.GetClientData(hs, h);
    if (fav_icon_available) {
      // The decoder will leave our bitmap empty on error.
      PNGDecoder::Decode(&data->data, &(fav_icon_map_[index]));

      // Notify the observer.
      if (!fav_icon_map_[index].isNull() && observer_)
        observer_->OnItemsChanged(static_cast<int>(index), 1);
    }
  }
}
