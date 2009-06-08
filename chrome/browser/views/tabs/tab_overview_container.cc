// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_container.h"

#include <gtk/gtk.h>

#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "chrome/browser/views/tabs/tab_overview_grid.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/widget/widget_gtk.h"

// Padding between the edges of us and the grid.
static const int kVerticalPadding = 43;
static const int kHorizontalPadding = 30;

// Height of the arrow.
static const int kArrowHeight = 28;

// Inset of the slight rounding on the corners.
static const int kEdgeInset = 5;

TabOverviewContainer::TabOverviewContainer() {
}

TabOverviewContainer::~TabOverviewContainer() {
}

void TabOverviewContainer::SetMaxSize(const gfx::Size& max_size) {
  GetTabOverviewGrid()->set_max_size(
      gfx::Size(max_size.width() - kHorizontalPadding * 2,
                max_size.height() - kVerticalPadding * 2 - kArrowHeight));
}

void TabOverviewContainer::UpdateWidgetShape(int width, int height) {
  int bottom_y = height - kArrowHeight;
  int right_edge = width - 1;
  int center = width / 2;
  // The points in alternating x,y pairs.
  int points[] = {
      kEdgeInset, 0,
      right_edge - kEdgeInset, 0,
      right_edge, kEdgeInset,
      right_edge, bottom_y - kEdgeInset - 1,
      right_edge - kEdgeInset, bottom_y - 1,
      center + kArrowHeight / 2, bottom_y - 1,
      center, bottom_y - 1 +  kArrowHeight,
      center - kArrowHeight / 2, bottom_y - 1,
      kEdgeInset, bottom_y - 1,
      0, bottom_y - 1 - kEdgeInset,
      0, kEdgeInset,
  };
  gfx::Path path;
  path.moveTo(SkIntToScalar(points[0]), SkIntToScalar(points[1]));
  for (size_t i = 2; i < arraysize(points); i += 2)
    path.lineTo(SkIntToScalar(points[i]), SkIntToScalar(points[i + 1]));

  GetWidget()->SetShape(path);
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

void TabOverviewContainer::DidChangeBounds(const gfx::Rect& previous,
                                           const gfx::Rect& current) {
  Layout();
}

TabOverviewGrid* TabOverviewContainer::GetTabOverviewGrid() {
  return static_cast<TabOverviewGrid*>(GetChildViewAt(0));
}
