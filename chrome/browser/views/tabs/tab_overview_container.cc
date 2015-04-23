// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_overview_container.h"

#include "app/gfx/canvas.h"
#include "chrome/browser/views/tabs/tab_overview_grid.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

// Padding between the edges of us and the grid.
static const int kVerticalPadding = 43;
static const int kHorizontalPadding = 30;

// Height of the arrow.
static const int kArrowHeight = 28;

// Radius of the corners of the rectangle.
static const int kEdgeSize = 8;

TabOverviewContainer::TabOverviewContainer() : arrow_center_(0) {
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
  // Create a path with a rounded rect and arrow pointing down.
  SkPath outline;
  int right = width() - 1;
  int bottom = height() - 1 - kArrowHeight;
  outline.moveTo(SkIntToScalar(kEdgeSize), SkIntToScalar(0));
  outline.arcTo(SkIntToScalar(right), SkIntToScalar(0),
                SkIntToScalar(right), SkIntToScalar(bottom),
                SkIntToScalar(kEdgeSize));
  outline.arcTo(SkIntToScalar(right), SkIntToScalar(bottom),
                SkIntToScalar(0), SkIntToScalar(bottom),
                SkIntToScalar(kEdgeSize));
  // Convert arrow_center to our coordinates.
  int arrow_center = arrow_center_ - bounds().x();
  if (arrow_center >= kArrowHeight && arrow_center < width() - kArrowHeight) {
    // Only draw the arrow if we have enough space.
    outline.lineTo(SkIntToScalar(arrow_center + kArrowHeight / 2),
                   SkIntToScalar(bottom));
    outline.lineTo(SkIntToScalar(arrow_center),
                   SkIntToScalar(bottom + kArrowHeight));
    outline.lineTo(SkIntToScalar(arrow_center - kArrowHeight / 2),
                   SkIntToScalar(bottom));
  }
  outline.arcTo(SkIntToScalar(0), SkIntToScalar(bottom),
                SkIntToScalar(0), SkIntToScalar(0),
                SkIntToScalar(kEdgeSize));
  outline.arcTo(SkIntToScalar(0), SkIntToScalar(0),
                SkIntToScalar(right), SkIntToScalar(0),
                SkIntToScalar(kEdgeSize));

  canvas->save();
  // Clip out the outline.
  canvas->clipPath(outline);

  // Fill the interior with a gradient.
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
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  canvas->drawPaint(paint);

  // Restore the canvas (resetting the clip).
  canvas->restore();

  SkPaint paint2;
  paint2.setStyle(SkPaint::kStroke_Style);
  paint2.setAntiAlias(true);
  paint2.setColor(SK_ColorWHITE);
  paint2.setStrokeWidth(SkIntToScalar(0));

  // And stroke the rounded rect with arrow pointing down.
  canvas->drawPath(outline, paint2);
}

TabOverviewGrid* TabOverviewContainer::GetTabOverviewGrid() {
  return static_cast<TabOverviewGrid*>(GetChildViewAt(0));
}
