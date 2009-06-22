// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_container.h"

#include "app/gfx/canvas.h"
#include "chrome/browser/views/tabs/tab_overview_grid.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

// Padding between the edges of us and the grid.
static const int kVerticalPadding = 43;
static const int kHorizontalPadding = 30;

// Height of the arrow.
static const int kArrowHeight = 28;

TabOverviewContainer::TabOverviewContainer() {
}

TabOverviewContainer::~TabOverviewContainer() {
}

void TabOverviewContainer::SetMaxSize(const gfx::Size& max_size) {
  GetTabOverviewGrid()->set_max_size(
      gfx::Size(max_size.width() - kHorizontalPadding * 2,
                max_size.height() - kVerticalPadding * 2 - kArrowHeight));
}

gfx::Size TabOverviewContainer::GetPreferredSize() {
  gfx::Size tab_overview_pref = GetTabOverviewGrid()->GetPreferredSize();
  return gfx::Size(kHorizontalPadding * 2 + tab_overview_pref.width(),
                   kVerticalPadding * 2 + tab_overview_pref.height() +
                   kArrowHeight);
}

void TabOverviewContainer::Layout() {
  GetTabOverviewGrid()->SetBounds(kHorizontalPadding, kVerticalPadding,
                                  width() - kHorizontalPadding * 2,
                                  height() - kVerticalPadding * 2 -
                                  kArrowHeight);
}

void TabOverviewContainer::Paint(gfx::Canvas* canvas) {
  SkPoint points[] = { { SkIntToScalar(0), SkIntToScalar(0) },
                       { SkIntToScalar(0), SkIntToScalar(height()) } };
  SkColor colors[] = { SkColorSetARGB(242, 255, 255, 255),
                       SkColorSetARGB(212, 255, 255, 255), };
  SkShader* shader = SkGradientShader::CreateLinear(
      points, colors, NULL, 2, SkShader::kRepeat_TileMode);
  SkPaint paint;
  paint.setShader(shader);
  shader = NULL;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setPorterDuffXfermode(SkPorterDuff::kSrcOver_Mode);
  canvas->drawPaint(paint);
}

TabOverviewGrid* TabOverviewContainer::GetTabOverviewGrid() {
  return static_cast<TabOverviewGrid*>(GetChildViewAt(0));
}
