// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/views/background.h"

#include "base/gfx/skia_utils.h"
#include "base/logging.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/painter.h"
#include "chrome/views/view.h"
#include "skia/include/SkPaint.h"

namespace ChromeViews {

// SolidBackground is a trivial Background implementation that fills the
// background in a solid color.
class SolidBackground : public Background {
 public:
  explicit SolidBackground(const SkColor& color) :
      color_(color) {
    SetNativeControlColor(color_);
  }

  void Paint(ChromeCanvas* canvas, View* view) const {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->drawColor(color_);
  }

 private:
  const SkColor color_;

  DISALLOW_EVIL_CONSTRUCTORS(SolidBackground);
};

class BackgroundPainter : public ChromeViews::Background {
 public:
  BackgroundPainter(bool owns_painter, Painter* painter)
      : owns_painter_(owns_painter), painter_(painter) {
    DCHECK(painter);
  }

  virtual ~BackgroundPainter() {
    if (owns_painter_)
      delete painter_;
  }


  void Paint(ChromeCanvas* canvas, View* view) const {
    Painter::PaintPainterAt(0, 0, view->GetWidth(), view->GetHeight(), canvas,
                            painter_);
  }

 private:
  bool owns_painter_;
  Painter* painter_;

  DISALLOW_EVIL_CONSTRUCTORS(BackgroundPainter);
};

Background::Background() : native_control_brush_(NULL) {
}

Background::~Background() {
  DeleteObject(native_control_brush_);
}

void Background::SetNativeControlColor(SkColor color) {
  DeleteObject(native_control_brush_);
  native_control_brush_ = CreateSolidBrush(gfx::SkColorToCOLORREF(color));
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

}
