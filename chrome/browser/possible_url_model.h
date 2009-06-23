// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POSSIBLE_URL_MODEL_H_
#define CHROME_BROWSER_POSSIBLE_URL_MODEL_H_

#include <string>
#include <vector>

#include "app/gfx/text_elider.h"
#include "app/table_model.h"
#include "base/string_util.h"
#include "chrome/browser/history/history.h"
#include "third_party/skia/include/core/SkBitmap.h"

////////////////////////////////////////////////////////////////////////////////
//
// A table model to represent the list of URLs that the user might want to
// bookmark.
//
////////////////////////////////////////////////////////////////////////////////

class PossibleURLModel : public TableModel {
 public:
  PossibleURLModel();

  virtual ~PossibleURLModel() {
  }

  void Reload(Profile *profile);

  void OnHistoryQueryComplete(HistoryService::Handle h,
                              history::QueryResults* result);

  virtual int RowCount() {
    return static_cast<int>(results_.size());
  }

  const GURL& GetURL(int row);

  const std::wstring& GetTitle(int row);

  virtual std::wstring GetText(int row, int col_id);

  virtual SkBitmap GetIcon(int row);

  virtual int CompareValues(int row1, int row2, int column_id);

  virtual void OnFavIconAvailable(HistoryService::Handle h,
                                  bool fav_icon_available,
                                  scoped_refptr<RefCountedBytes> data,
                                  bool expired,
                                  GURL icon_url);

  virtual void SetObserver(TableModelObserver* observer) {
    observer_ = observer;
  }

 private:
  // Contains the data needed to show a result.
  struct Result {
    Result() : index(0) {}

    GURL url;
    // Index of this Result in results_. This is used as the key into
    // fav_icon_map_ to lookup the favicon for the url, as well as the index
    // into results_ when the favicon is received.
    size_t index;
    gfx::SortedDisplayURL display_url;
    std::wstring title;
  };

  // The current profile.
  Profile* profile_;

  // Our observer.
  TableModelObserver* observer_;

  // Our consumer for favicon requests.
  CancelableRequestConsumerT<size_t, NULL> consumer_;

  // The results we're showing.
  std::vector<Result> results_;

  // Map Result::index -> Favicon.
  typedef std::map<size_t, SkBitmap> FavIconMap;
  FavIconMap fav_icon_map_;

  DISALLOW_EVIL_CONSTRUCTORS(PossibleURLModel);
};

#endif  // CHROME_BROWSER_POSSIBLE_URL_MODEL_H_
