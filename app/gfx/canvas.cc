// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"

#include <limits>

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkShader.h"

namespace gfx {

bool Canvas::GetClipRect(gfx::Rect* r) {
  SkRect clip;
  if (!getClipBounds(&clip)) {
    if (r)
      r->SetRect(0, 0, 0, 0);
    return false;
  }
  r->SetRect(SkScalarRound(clip.fLeft), SkScalarRound(clip.fTop),
             SkScalarRound(clip.fRight - clip.fLeft),
             SkScalarRound(clip.fBottom - clip.fTop));
  return true;
}

bool Canvas::ClipRectInt(int x, int y, int w, int h) {
  SkRect new_clip;
  new_clip.set(SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(x + w), SkIntToScalar(y + h));
  return clipRect(new_clip);
}

bool Canvas::IntersectsClipRectInt(int x, int y, int w, int h) {
  SkRect clip;
  return getClipBounds(&clip) &&
      clip.intersect(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(x + w),
                     SkIntToScalar(y + h));
}

void Canvas::TranslateInt(int x, int y) {
  translate(SkIntToScalar(x), SkIntToScalar(y));
}

void Canvas::ScaleInt(int x, int y) {
  scale(SkIntToScalar(x), SkIntToScalar(y));
}

void Canvas::FillRectInt(const SkColor& color, int x, int y, int w, int h) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  FillRectInt(x, y, w, h, paint);
}

void Canvas::FillRectInt(int x, int y, int w, int h, const SkPaint& paint) {
  SkIRect rc = {x, y, x + w, y + h};
  drawIRect(rc, paint);
}

void Canvas::DrawRectInt(const SkColor& color, int x, int y, int w, int h) {
  DrawRectInt(color, x, y, w, h, SkXfermode::kSrcOver_Mode);
}

void Canvas::DrawRectInt(const SkColor& color, int x, int y, int w, int h,
                         SkXfermode::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kStroke_Style);
  // Set a stroke width of 0, which will put us down the stroke rect path.  If
  // we set a stroke width of 1, for example, this will internally create a
  // path and fill it, which causes problems near the edge of the canvas.
  paint.setStrokeWidth(SkIntToScalar(0));
  paint.setXfermodeMode(mode);

  SkIRect rc = {x, y, x + w, y + h};
  drawIRect(rc, paint);
}

void Canvas::DrawLineInt(const SkColor& color, int x1, int y1, int x2, int y2) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStrokeWidth(SkIntToScalar(1));
  drawLine(SkIntToScalar(x1), SkIntToScalar(y1), SkIntToScalar(x2),
           SkIntToScalar(y2), paint);
}

void Canvas::DrawFocusRect(int x, int y, int width, int height) {
  // Create a 2D bitmap containing alternating on/off pixels - we do this
  // so that you never get two pixels of the same color around the edges
  // of the focus rect (this may mean that opposing edges of the rect may
  // have a dot pattern out of phase to each other).
  static SkBitmap* dots = NULL;
  if (!dots) {
    int col_pixels = 32;
    int row_pixels = 32;

    dots = new SkBitmap;
    dots->setConfig(SkBitmap::kARGB_8888_Config, col_pixels, row_pixels);
    dots->allocPixels();
    dots->eraseARGB(0, 0, 0, 0);

    uint32_t* dot = dots->getAddr32(0, 0);
    for (int i = 0; i < row_pixels; i++) {
      for (int u = 0; u < col_pixels; u++) {
        if ((u % 2 + i % 2) % 2 != 0) {
          dot[i * row_pixels + u] = SK_ColorGRAY;
        }
      }
    }
  }

  // First the horizontal lines.

  // Make a shader for the bitmap with an origin of the box we'll draw. This
  // shader is refcounted and will have an initial refcount of 1.
  SkShader* shader = SkShader::CreateBitmapShader(
      *dots, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);
  // Assign the shader to the paint & release our reference. The paint will
  // now own the shader and the shader will be destroyed when the paint goes
  // out of scope.
  SkPaint paint;
  paint.setShader(shader);
  shader->unref();

  SkRect rect;
  rect.set(SkIntToScalar(x), SkIntToScalar(y),
           SkIntToScalar(x + width), SkIntToScalar(y + 1));
  drawRect(rect, paint);
  rect.set(SkIntToScalar(x), SkIntToScalar(y + height - 1),
           SkIntToScalar(x + width), SkIntToScalar(y + height));
  drawRect(rect, paint);

  rect.set(SkIntToScalar(x), SkIntToScalar(y),
           SkIntToScalar(x + 1), SkIntToScalar(y + height));
  drawRect(rect, paint);
  rect.set(SkIntToScalar(x + width - 1), SkIntToScalar(y),
           SkIntToScalar(x + width), SkIntToScalar(y + height));
  drawRect(rect, paint);
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap, int x, int y) {
  drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y));
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap, int x, int y,
                           const SkPaint& paint) {
  drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y), &paint);
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y,
                           int src_w, int src_h, int dest_x, int dest_y,
                           int dest_w, int dest_h,
                           bool filter) {
  SkPaint p;
  DrawBitmapInt(bitmap, src_x, src_y, src_w, src_h, dest_x, dest_y,
                dest_w, dest_h, filter, p);
}

void Canvas::DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y,
                           int src_w, int src_h, int dest_x, int dest_y,
                           int dest_w, int dest_h,
                           bool filter, const SkPaint& paint) {
  DLOG_ASSERT(src_x + src_w < std::numeric_limits<int16_t>::max() &&
              src_y + src_h < std::numeric_limits<int16_t>::max());
  if (src_w <= 0 || src_h <= 0 || dest_w <= 0 || dest_h <= 0) {
    NOTREACHED() << "Attempting to draw bitmap to/from an empty rect!";
    return;
  }

  if (!IntersectsClipRectInt(dest_x, dest_y, dest_w, dest_h))
    return;

  SkRect dest_rect = { SkIntToScalar(dest_x),
                       SkIntToScalar(dest_y),
                       SkIntToScalar(dest_x + dest_w),
                       SkIntToScalar(dest_y + dest_h) };

  if (src_w == dest_w && src_h == dest_h) {
    // Workaround for apparent bug in Skia that causes image to occasionally
    // shift.
    SkIRect src_rect = { src_x, src_y, src_x + src_w, src_y + src_h };
    drawBitmapRect(bitmap, &src_rect, dest_rect, &paint);
    return;
  }

  // Make a bitmap shader that contains the bitmap we want to draw. This is
  // basically what SkCanvas.drawBitmap does internally, but it gives us
  // more control over quality and will use the mipmap in the source image if
  // it has one, whereas drawBitmap won't.
  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  SkMatrix shader_scale;
  shader_scale.setScale(SkFloatToScalar(static_cast<float>(dest_w) / src_w),
                        SkFloatToScalar(static_cast<float>(dest_h) / src_h));
  shader_scale.preTranslate(SkIntToScalar(-src_x), SkIntToScalar(-src_y));
  shader_scale.postTranslate(SkIntToScalar(dest_x), SkIntToScalar(dest_y));
  shader->setLocalMatrix(shader_scale);

  // Set up our paint to use the shader & release our reference (now just owned
  // by the paint).
  SkPaint p(paint);
  p.setFilterBitmap(filter);
  p.setShader(shader);
  shader->unref();

  // The rect will be filled by the bitmap.
  drawRect(dest_rect, p);
}

void Canvas::DrawStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           const SkColor& color,
                           int x, int y, int w, int h) {
  DrawStringInt(text, font, color, x, y, w, h,
                l10n_util::DefaultCanvasTextAlignment());
}

#if defined(OS_MACOSX)
void Canvas::DrawStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           const SkColor& color,
                           int x, int y, int w, int h,
                           int flags) {
  NOTIMPLEMENTED();
}
#endif

void Canvas::TileImageInt(const SkBitmap& bitmap, int x, int y, int w, int h) {
  TileImageInt(bitmap, 0, 0, x, y, w, h);
}

void Canvas::TileImageInt(const SkBitmap& bitmap, int src_x, int src_y,
                          int dest_x, int dest_y, int w, int h) {
  if (!IntersectsClipRectInt(dest_x, dest_y, w, h))
    return;

  SkPaint paint;

  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  paint.setShader(shader);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);

  // CreateBitmapShader returns a Shader with a reference count of one, we
  // need to unref after paint takes ownership of the shader.
  shader->unref();
  save();
  translate(SkIntToScalar(dest_x - src_x), SkIntToScalar(dest_y - src_y));
  ClipRectInt(src_x, src_y, w, h);
  drawPaint(paint);
  restore();
}

SkBitmap Canvas::ExtractBitmap() {
  const SkBitmap& device_bitmap = getDevice()->accessBitmap(false);

  // Make a bitmap to return, and a canvas to draw into it. We don't just want
  // to call extractSubset or the copy constuctor, since we want an actual copy
  // of the bitmap.
  SkBitmap result;
  device_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);
  return result;
}

}  // namespace gfx
