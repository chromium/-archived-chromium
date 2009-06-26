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

  // Sets the preferred size. Normally the preferred size is calculate from
  // the content, but this can be used to fix it at a particular value. Use an
  // empty size to get the default preferred size.
  void set_preferred_size(const gfx::Size& preferred_size) {
    preferred_size_ = preferred_size;
  }

  // Returns true if the specified point, in the bounds of the cell, is over
  // the thumbnail.
  bool IsPointInThumbnail(const gfx::Point& point);

  // Has the thumbnail been configured? This is true after SetThumbnail
  // is invoked.
  bool configured_thumbnail() const { return configured_thumbnail_; }

  // View overrides.
  virtual gfx::Size GetPreferredSize();

 private:
  views::Label* title_label_;
  views::ImageView* thumbnail_view_;
  views::ImageView* fav_icon_view_;

  // Specific preferred size. See set_preferred_size() for details.
  gfx::Size preferred_size_;

  bool configured_thumbnail_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewCell);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_CELL_H_
