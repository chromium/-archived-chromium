// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_cell.h"

#include "app/gfx/favicon_size.h"
#include "base/string_util.h"
#include "skia/ext/image_operations.h"
#include "views/border.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"

using views::ColumnSet;
using views::GridLayout;

// Padding between the favicon and label.
static const int kFavIconPadding = 4;

// Height of the thumbnail.
static const int kThumbnailHeight = 140;
static const int kThumbnailWidth = 220;

// Padding between favicon/title and thumbnail.
static const int kVerticalPadding = 10;

TabOverviewCell::TabOverviewCell() : configured_thumbnail_(false) {
  title_label_ = new views::Label();
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  thumbnail_view_ = new views::ImageView();
  thumbnail_view_->SetImageSize(gfx::Size(kThumbnailWidth, kThumbnailHeight));

  fav_icon_view_ = new views::ImageView();
  fav_icon_view_->SetImageSize(gfx::Size(kFavIconSize, kFavIconSize));

  int title_cs_id = 0;
  int thumbnail_cs_id = 1;
  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);
  ColumnSet* columns = layout->AddColumnSet(title_cs_id);
  columns->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                     GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kFavIconPadding);
  columns->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                     GridLayout::USE_PREF, 0, 0);

  columns = layout->AddColumnSet(thumbnail_cs_id);
  columns->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                     GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, title_cs_id);
  layout->AddView(fav_icon_view_);
  layout->AddView(title_label_);

  layout->StartRowWithPadding(1, thumbnail_cs_id, 0, kVerticalPadding);
  layout->AddView(thumbnail_view_);

  thumbnail_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));

  thumbnail_view_->set_border(
      views::Border::CreateSolidBorder(1, SkColorSetRGB(176, 176, 176)));
}

void TabOverviewCell::SetThumbnail(const SkBitmap& thumbnail) {
  // Do mipmapped-based resampling to get closer to the correct size. The
  // input bitmap isn't guaranteed to have any specific resolution.
  thumbnail_view_->SetImage(skia::ImageOperations::DownsampleByTwoUntilSize(
      thumbnail, kThumbnailWidth, kThumbnailHeight));
  configured_thumbnail_ = true;
}

void TabOverviewCell::SetTitle(const string16& title) {
  title_label_->SetText(UTF16ToWide(title));
}

void TabOverviewCell::SetFavIcon(const SkBitmap& favicon) {
  fav_icon_view_->SetImage(favicon);
}

bool TabOverviewCell::IsPointInThumbnail(const gfx::Point& point) {
  return thumbnail_view_->bounds().Contains(point);
}

gfx::Size TabOverviewCell::GetPreferredSize() {
  if (!preferred_size_.IsEmpty())
    return preferred_size_;

  // Force the preferred width to that of the thumbnail.
  gfx::Size thumbnail_pref = thumbnail_view_->GetPreferredSize();
  gfx::Size pref = View::GetPreferredSize();
  pref.set_width(thumbnail_pref.width());
  return pref;
}
