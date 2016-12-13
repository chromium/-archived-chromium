// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/background.h"

#include "app/gfx/canvas.h"
#include "base/logging.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "views/painter.h"
#include "views/view.h"

namespace views {

// SolidBackground is a trivial Background implementation that fills the
// background in a solid color.
class SolidBackground : public Background {
 public:
  explicit SolidBackground(const SkColor& color) :
      color_(color) {
    SetNativeControlColor(color_);
  }

  void Paint(gfx::Canvas* canvas, View* view) const {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->drawColor(color_);
  }

 private:
  const SkColor color_;

  DISALLOW_EVIL_CONSTRUCTORS(SolidBackground);
};

class BackgroundPainter : public Background {
 public:
  BackgroundPainter(bool owns_painter, Painter* painter)
      : owns_painter_(owns_painter), painter_(painter) {
    DCHECK(painter);
  }

  virtual ~BackgroundPainter() {
    if (owns_painter_)
      delete painter_;
  }


  void Paint(gfx::Canvas* canvas, View* view) const {
    Painter::PaintPainterAt(0, 0, view->width(), view->height(), canvas,
                            painter_);
  }

 private:
  bool owns_painter_;
  Painter* painter_;

  DISALLOW_EVIL_CONSTRUCTORS(BackgroundPainter);
};

Background::Background()
#if defined(OS_WIN)
    : native_control_brush_(NULL)
#endif
{
}

Background::~Background() {
#if defined(OS_WIN)
  DeleteObject(native_control_brush_);
#endif
}

void Background::SetNativeControlColor(SkColor color) {
#if defined(OS_WIN)
  DeleteObject(native_control_brush_);
  native_control_brush_ = CreateSolidBrush(skia::SkColorToCOLORREF(color));
#endif
}

//static
Background* Background::CreateSolidBackground(const SkColor& color) {
  return new SolidBackground(color);
}

//static
Background* Background::CreateStandardPanelBackground() {
  return CreateVerticalGradientBackground(SkColorSetRGB(246, 250, 255),
                                          SkColorSetRGB(219, 235, 255));
}

//static
Background* Background::CreateVerticalGradientBackground(
    const SkColor& color1, const SkColor& color2) {
    Background *background =
        CreateBackgroundPainter(true, Painter::CreateVerticalGradient(color1,
                                                                      color2));
    background->SetNativeControlColor(  // 50% blend of colors 1 & 2
        SkColorSetRGB((SkColorGetR(color1) + SkColorGetR(color2)) / 2,
                      (SkColorGetG(color1) + SkColorGetG(color2)) / 2,
                      (SkColorGetB(color1) + SkColorGetB(color2)) / 2));

    return background;
}

//static
Background* Background::CreateBackgroundPainter(bool owns_painter,
                                                Painter* painter) {
  return new BackgroundPainter(owns_painter, painter);
}

}  // namespace views
