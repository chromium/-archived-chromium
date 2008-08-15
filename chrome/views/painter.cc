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

#include "chrome/views/painter.h"

#include "base/logging.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/resource_bundle.h"
#include "skia/include/SkBitmap.h"
#include "skia/include/SkGradientShader.h"

namespace ChromeViews {

namespace {

class GradientPainter : public Painter {
 public:
  GradientPainter(bool horizontal, const SkColor& top, const SkColor& bottom)
      : horizontal_(horizontal) {
    colors_[0] = top;
    colors_[1] = bottom;
  }

  virtual ~GradientPainter() {
  }

  void Paint(int w, int h, ChromeCanvas* canvas) {
    SkPaint paint;
    SkPoint p[2];
    p[0].set(SkIntToScalar(0), SkIntToScalar(0));
    if (horizontal_)
      p[1].set(SkIntToScalar(w), SkIntToScalar(0));
    else
      p[1].set(SkIntToScalar(0), SkIntToScalar(h));

    SkShader* s =
        SkGradientShader::CreateLinear(p, colors_, NULL, 2,
                                       SkShader::kClamp_TileMode, NULL);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();

    canvas->drawRectCoords(SkIntToScalar(0), SkIntToScalar(0),
                           SkIntToScalar(w), SkIntToScalar(h), paint);
  }

 private:
  bool horizontal_;
  SkColor colors_[2];

  DISALLOW_EVIL_CONSTRUCTORS(GradientPainter);
};


}

// static
void Painter::PaintPainterAt(int x, int y, int w, int h,
                             ChromeCanvas* canvas, Painter* painter) {
  DCHECK(canvas && painter);
  if (w < 0 || h < 0)
    return;
  canvas->save();
  canvas->TranslateInt(x, y);
  painter->Paint(w, h, canvas);
  canvas->restore();
}

ImagePainter::ImagePainter(const int image_resource_names[],
                           bool draw_center)
    : draw_center_(draw_center) {
  DCHECK(image_resource_names);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  for (int i = 0, max = draw_center ? 9 : 8; i < max; ++i)
    images_.push_back(rb.GetBitmapNamed(image_resource_names[i]));
}

void ImagePainter::Paint(int w, int h, ChromeCanvas* canvas) {
  canvas->DrawBitmapInt(*images_[BORDER_TOP_LEFT], 0, 0);
  canvas->TileImageInt(*images_[BORDER_TOP],
                       images_[BORDER_TOP_LEFT]->width(),
                       0,
                       w - images_[BORDER_TOP_LEFT]->width() -
                       images_[BORDER_TOP_RIGHT]->width(),
                       images_[BORDER_TOP_LEFT]->height());
  canvas->DrawBitmapInt(*images_[BORDER_TOP_RIGHT],
                        w - images_[BORDER_TOP_RIGHT]->width(),
                        0);
  canvas->TileImageInt(*images_[BORDER_RIGHT],
                       w - images_[BORDER_RIGHT]->width(),
                       images_[BORDER_TOP_RIGHT]->height(),
                       images_[BORDER_RIGHT]->width(),
                       h - images_[BORDER_TOP_RIGHT]->height() -
                       images_[BORDER_BOTTOM_RIGHT]->height());
  canvas->DrawBitmapInt(*images_[BORDER_BOTTOM_RIGHT],
                        w - images_[BORDER_BOTTOM_RIGHT]->width(),
                        h - images_[BORDER_BOTTOM_RIGHT]->height());
  canvas->TileImageInt(*images_[BORDER_BOTTOM],
                       images_[BORDER_BOTTOM_LEFT]->width(),
                       h - images_[BORDER_BOTTOM]->height(),
                       w - images_[BORDER_BOTTOM_LEFT]->width() -
                       images_[BORDER_BOTTOM_RIGHT]->width(),
                       images_[BORDER_BOTTOM]->height());
  canvas->DrawBitmapInt(*images_[BORDER_BOTTOM_LEFT],
                        0,
                        h - images_[BORDER_BOTTOM_LEFT]->height());
  canvas->TileImageInt(*images_[BORDER_LEFT],
                       0,
                       images_[BORDER_TOP_LEFT]->height(),
                       images_[BORDER_LEFT]->width(),
                       h - images_[BORDER_TOP_LEFT]->height() -
                       images_[BORDER_BOTTOM_LEFT]->height());
  if (draw_center_) {
    canvas->DrawBitmapInt(*images_[BORDER_CENTER],
                          0,
                          0,
                          images_[BORDER_CENTER]->width(),
                          images_[BORDER_CENTER]->height(),
                          images_[BORDER_TOP_LEFT]->width(),
                          images_[BORDER_TOP_LEFT]->height(),
                          w - images_[BORDER_TOP_LEFT]->width() -
                              images_[BORDER_TOP_RIGHT]->width(),
                          h - images_[BORDER_TOP_LEFT]->height() -
                              images_[BORDER_TOP_RIGHT]->height(),
                          false);
  }
}

// static
Painter* Painter::CreateHorizontalGradient(SkColor c1, SkColor c2) {
  return new GradientPainter(true, c1, c2);
}

// static
Painter* Painter::CreateVerticalGradient(SkColor c1, SkColor c2) {
  return new GradientPainter(false, c1, c2);
}

HorizontalPainter::HorizontalPainter(const int image_resource_names[]) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  for (int i = 0; i < 3; ++i)
    images_[i] = rb.GetBitmapNamed(image_resource_names[i]);
  height_ = images_[LEFT]->height();
  DCHECK(images_[LEFT]->height() == images_[RIGHT]->height() &&
         images_[LEFT]->height() == images_[CENTER]->height());
}

void HorizontalPainter::Paint(int w, int h, ChromeCanvas* canvas) {
  if (w < (images_[LEFT]->width() + images_[CENTER]->width() +
            images_[RIGHT]->width())) {
    // No room to paint.
    return;
  }
  canvas->DrawBitmapInt(*images_[LEFT], 0, 0);
  canvas->DrawBitmapInt(*images_[RIGHT], w - images_[RIGHT]->width(), 0);
  canvas->TileImageInt(*images_[CENTER],
                       images_[LEFT]->width(),
                       0,
                       w - images_[LEFT]->width() - images_[RIGHT]->width(),
                       height_);
}

}
