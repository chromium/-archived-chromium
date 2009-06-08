// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CELL_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CELL_H_

#include "base/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/view.h"

namespace views {
class ImageView;
class Label;
}

// A single cell displayed by TabOverviewGrid. TabOverviewCell contains a
// label, favicon and thumnbail.
class TabOverviewCell : public views::View {
 public:
  TabOverviewCell();

  void SetThumbnail(const SkBitmap& thumbnail);
  void SetTitle(const string16& title);
  void SetFavIcon(const SkBitmap& favicon);

  // Returns true if the specified point, in the bounds of the cell, is over
  // the thumbnail.
  bool IsPointInThumbnail(const gfx::Point& point);

  // View overrides.
  virtual gfx::Size GetPreferredSize();

 private:
  views::Label* title_label_;
  views::ImageView* thumbnail_view_;
  views::ImageView* fav_icon_view_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewCell);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CELL_H_
