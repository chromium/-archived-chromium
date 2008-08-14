// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "SkGraphicsContext.h"

#include <vssym32.h>

#include "base/gfx/image_operations.h"
#include "base/gfx/native_theme.h"
#include "base/gfx/platform_canvas_win.h"
#include "base/gfx/skia_utils.h"
#include "GraphicsContextPrivate.h"
#include "NativeImageSkia.h"
#include "SkBitmap.h"

// These need to be moved
#include "ThemeData.h"
#include "UniscribeStateTextRun.h"

#undef LOG
#include "SkiaUtils.h"


namespace {

// Draws the given bitmap to the given canvas. The subset of the source bitmap
// identified by src_rect is drawn to the given destination rect. The bitmap
// will be resampled to resample_width * resample_height (this is the size of
// the whole image, not the subset). See shouldResampleBitmap for more.
//
// This does a lot of computation to resample only the portion of the bitmap
// that will only be drawn. This is critical for performance since when we are
// scrolling, for example, we are only drawing a small strip of the image.
// Resampling the whole image every time is very slow, so this speeds up things
// dramatically.
void DrawResampledBitmap(SkCanvas& canvas,
                         SkPaint& paint,
                         const NativeImageSkia& bitmap,
                         const SkIRect& src_irect,
                         const SkRect& dest_rect) {
  // First get the subset we need. This is efficient and does not copy pixels.
  SkBitmap subset;
  bitmap.extractSubset(&subset, src_irect);
  SkRect src_rect;
  src_rect.set(src_irect);

  // Whether we're doing a subset or using the full source image.
  bool src_is_full = src_irect.fLeft == 0 && src_irect.fTop == 0 &&
      src_irect.width() == bitmap.width() &&
      src_irect.height() == bitmap.height();

  // We will always draw in integer sizes, so round the destination rect.
  SkIRect dest_rect_rounded;
  dest_rect.round(&dest_rect_rounded);
  SkIRect resized_image_rect;  // Represents the size of the resized image.
  resized_image_rect.set(0, 0,
                         dest_rect_rounded.width(), dest_rect_rounded.height());

  if (src_is_full &&
      bitmap.hasResizedBitmap(dest_rect_rounded.width(),
                              dest_rect_rounded.height())) {
    // Yay, this bitmap frame already has a resized version appropriate for us.
    SkBitmap resampled = bitmap.resizedBitmap(dest_rect_rounded.width(),
                                              dest_rect_rounded.height());
    canvas.drawBitmapRect(resampled, NULL, dest_rect, &paint);
    return;
  }

  // Compute the visible portion of our rect.
  SkRect dest_bitmap_subset_sk;
  ClipRectToCanvas(canvas, dest_rect, &dest_bitmap_subset_sk);
  dest_bitmap_subset_sk.offset(-dest_rect.fLeft, -dest_rect.fTop);

  // The matrix inverting, etc. could have introduced rounding error which
  // causes the bounds to be outside of the resized bitmap. We round outward so
  // we always lean toward it being larger rather than smaller than we need,
  // and then clamp to the bitmap bounds so we don't get any invalid data.
  SkIRect dest_bitmap_subset_sk_i;
  dest_bitmap_subset_sk.roundOut(&dest_bitmap_subset_sk_i);
  if (!dest_bitmap_subset_sk_i.intersect(resized_image_rect))
    return;  // Resized image does not intersect.

  if (src_is_full && bitmap.shouldCacheResampling(
          resized_image_rect.width(),
          resized_image_rect.height(),
          dest_bitmap_subset_sk_i.width(),
          dest_bitmap_subset_sk_i.height())) {
    // We're supposed to resize the entire image and cache it, even though we
    // don't need all of it.
    SkBitmap resampled = bitmap.resizedBitmap(dest_rect_rounded.width(),
                                              dest_rect_rounded.height());
    canvas.drawBitmapRect(resampled, NULL, dest_rect, &paint);
  } else {
    // We should only resize the exposed part of the bitmap to do the minimal
    // possible work.
    gfx::Rect dest_bitmap_subset(dest_bitmap_subset_sk_i.fLeft,
                                 dest_bitmap_subset_sk_i.fTop,
                                 dest_bitmap_subset_sk_i.width(),
                                 dest_bitmap_subset_sk_i.height());

    // Resample the needed part of the image.
    SkBitmap resampled = gfx::ImageOperations::Resize(subset,
        gfx::ImageOperations::RESIZE_LANCZOS3,
        gfx::Size(dest_rect_rounded.width(), dest_rect_rounded.height()),
        dest_bitmap_subset);

    // Compute where the new bitmap should be drawn. Since our new bitmap may be
    // smaller than the original, we have to shift it over by the same amount
    // that we cut off the top and left.
    SkRect offset_dest_rect = {
        dest_bitmap_subset.x() + dest_rect.fLeft,
        dest_bitmap_subset.y() + dest_rect.fTop,
        dest_bitmap_subset.right() + dest_rect.fLeft,
        dest_bitmap_subset.bottom() + dest_rect.fTop };

    canvas.drawBitmapRect(resampled, NULL, offset_dest_rect, &paint);
  }
}

}  // namespace


SkGraphicsContext::SkGraphicsContext(gfx::PlatformCanvasWin* canvas)
    : canvas_(canvas),
      paint_context_(NULL),
      own_canvas_(false) {
}

SkGraphicsContext::~SkGraphicsContext() {
  if (own_canvas_)
    delete canvas_;
}

const gfx::NativeTheme* SkGraphicsContext::nativeTheme() {
  return gfx::NativeTheme::instance();
}

void SkGraphicsContext::paintIcon(HICON icon, const SkIRect& rect) {
  HDC hdc = canvas_->beginPlatformPaint();
  DrawIconEx(hdc, rect.fLeft, rect.fTop, icon, rect.width(), rect.height(),
             0, 0, DI_NORMAL);
  canvas_->endPlatformPaint();
}

// RenderThemeWin.cpp
void SkGraphicsContext::paintButton(const SkIRect& widgetRect,
                                    const ThemeData& themeData) {
  RECT rect(gfx::SkIRectToRECT(widgetRect));
  HDC hdc = canvas_->beginPlatformPaint();
  int state = themeData.m_state;
  nativeTheme()->PaintButton(hdc,
                             themeData.m_part,
                             state,
                             themeData.m_classicState,
                             &rect);
  canvas_->endPlatformPaint();
}

void SkGraphicsContext::paintTextField(const SkIRect& widgetRect,
                                       const ThemeData& themeData,
                                       SkColor c, 
                                       bool drawEdges) {
  RECT rect(gfx::SkIRectToRECT(widgetRect));
  HDC hdc = canvas_->beginPlatformPaint();
  nativeTheme()->PaintTextField(hdc,
                                themeData.m_part,
                                themeData.m_state,
                                themeData.m_classicState,
                                &rect,
                                gfx::SkColorToCOLORREF(c),
                                true,
                                drawEdges);
  canvas_->endPlatformPaint();
}

void SkGraphicsContext::paintMenuListArrowButton(const SkIRect& widgetRect,
                                                 unsigned state,
                                                 unsigned classic_state) {
  RECT rect(gfx::SkIRectToRECT(widgetRect));
  HDC hdc = canvas_->beginPlatformPaint();
  nativeTheme()->PaintMenuList(hdc,
                               CP_DROPDOWNBUTTON,
                               state,
                               classic_state,
                               &rect);
  canvas_->endPlatformPaint();
}

void SkGraphicsContext::paintComplexText(UniscribeStateTextRun& state,
                                         const SkPoint& point,
                                         int from,
                                         int to,
                                         int ascent) {
  SkColor color(paint_context_->fillColor());
  uint8 alpha = SkColorGetA(color);
  // Skip 100% transparent text; no need to draw anything.
  if (!alpha)
    return;

  HDC hdc = canvas_->beginPlatformPaint();

  // TODO(maruel): http://b/700464 SetTextColor doesn't support transparency.
  // Enforce non-transparent color.
  color = SkColorSetRGB(SkColorGetR(color),
                        SkColorGetG(color),
                        SkColorGetB(color));
  SetTextColor(hdc, gfx::SkColorToCOLORREF(color));
  SetBkMode(hdc, TRANSPARENT);

  // Uniscribe counts the coordinates from the upper left, while WebKit uses
  // the baseline, so we have to subtract off the ascent.
  state.Draw(hdc,
             static_cast<int>(point.fX),
             static_cast<int>(point.fY - ascent),
             from,
             to);
  canvas_->endPlatformPaint();
}

bool SkGraphicsContext::paintText(HFONT hfont,
                                  int number_glyph,
                                  const WORD* glyphs,
                                  const int* advances,
                                  const SkPoint& origin) {
  SkColor color(paint_context_->fillColor());
  uint8 alpha = SkColorGetA(color);
  // Skip 100% transparent text; no need to draw anything.
  if (!alpha)
    return true;

  bool success = false;
  HDC hdc = canvas_->beginPlatformPaint();
  HGDIOBJ old_font = SelectObject(hdc, hfont);

  // TODO(maruel): http://b/700464 SetTextColor doesn't support transparency.
  // Enforce non-transparent color.
  color = SkColorSetRGB(SkColorGetR(color),
                        SkColorGetG(color),
                        SkColorGetB(color));
  SetTextColor(hdc, gfx::SkColorToCOLORREF(color));
  SetBkMode(hdc, TRANSPARENT);

  // The 'origin' represents the baseline, so we need to move it up to the
  // top of the bounding square by subtracting the ascent
  success = !!ExtTextOut(hdc, static_cast<int>(origin.fX),
                         static_cast<int>(origin.fY),
                         ETO_GLYPH_INDEX, NULL,
                         reinterpret_cast<const wchar_t*>(glyphs), number_glyph,
                         advances);
  SelectObject(hdc, old_font);
  canvas_->endPlatformPaint();
  return success;
}

void SkGraphicsContext::paintSkPaint(const SkRect& rect,
                                     const SkPaint& paint) {
  canvas_->drawRect(rect, paint);
}

// Returns smallest multiple of two of the dest size that is more than a small
// multiple larger than src_size.
//
// Used to determine the size that source should be high-quality upsampled to,
// after which we use linear interpolation. Making sure that the linear
// interpolation is a factor of two reduces artifacts, and doing the lowest
// level of resampling 
static int GetResamplingThreshold(int src_size, int dest_size) {
  int lower_bound = src_size * 3 / 2;  // Minimum size we'll resample to (1.5x).
  int cur = dest_size;

  // Find the largest multiple of two of the destination size less than our
  // threshold
  while (cur > lower_bound)
    cur /= 2;

  // We want the next size above that, or just the destination size if it's
  // smaller.
  cur *= 2;
  if (cur > dest_size)
    return dest_size;
  return cur;
}

// static
SkGraphicsContext::ResamplingMode SkGraphicsContext::computeResamplingMode(
    const NativeImageSkia& bitmap,
    int src_width, int src_height,
    float dest_width, float dest_height) {
  int dest_iwidth = static_cast<int>(dest_width);
  int dest_iheight = static_cast<int>(dest_height);

  // The percent change below which we will not resample. This usually means
  // an off-by-one error on the web page, and just doing nearest neighbor
  // sampling is usually good enough.
  const float kFractionalChangeThreshold = 0.025f;

  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  const int kSmallImageSizeThreshold = 8;

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  const float kLargeStretch = 3.0f;

  // Figure out if we should resample this image. We try to prune out some
  // common cases where resampling won't give us anything, since it is much
  // slower than drawing stretched.
  if (src_width == dest_iwidth && src_height == dest_iheight) {
    // We don't need to resample if the source and destination are the same.
    return RESAMPLE_NONE;
  }
  
  if (src_width <= kSmallImageSizeThreshold ||
    src_height <= kSmallImageSizeThreshold ||
    dest_width <= kSmallImageSizeThreshold ||
    dest_height <= kSmallImageSizeThreshold) {
    // Never resample small images. These are often used for borders and
    // rules (think 1x1 images used to make lines).
    return RESAMPLE_NONE;
  }
  
  if (src_height * kLargeStretch <= dest_height ||
    src_width * kLargeStretch <= dest_width) {
    // Large image detected.

    // Don't resample if it is being stretched a lot in only one direction.
    // This is trying to catch cases where somebody has created a border
    // (which might be large) and then is stretching it to fill some part
    // of the page.
    if (src_width == dest_width || src_height == dest_height)
      return RESAMPLE_NONE;

    // The image is growing a lot and in more than one direction. Resampling
    // is slow and doesn't give us very much when growing a lot.
    return RESAMPLE_LINEAR;
  }
  
  if ((fabs(dest_width - src_width) / src_width <
       kFractionalChangeThreshold) &&
     (fabs(dest_height - src_height) / src_height <
      kFractionalChangeThreshold)) {
    // It is disappointingly common on the web for image sizes to be off by
    // one or two pixels. We don't bother resampling if the size difference
    // is a small fraction of the original size.
    return RESAMPLE_NONE;
  }

  // When the image is not yet done loading, use linear. We don't cache the
  // partially resampled images, and as they come in incrementally, it causes
  // us to have to resample the whole thing every time.
  if (!bitmap.isDataComplete())
    return RESAMPLE_LINEAR;

  // Everything else gets resampled.
  return RESAMPLE_AWESOME;
}

void SkGraphicsContext::paintSkBitmap(const NativeImageSkia& bitmap,
                                      const SkIRect& src_rect,
                                      const SkRect& dest_rect,
                                      const SkPorterDuff::Mode& comp_op) {
  SkPaint paint;
  paint.setPorterDuffXfermode(comp_op);

  ResamplingMode resampling = IsPrinting() ? RESAMPLE_NONE :
      computeResamplingMode(bitmap, src_rect.width(), src_rect.height(),
                            SkScalarToFloat(dest_rect.width()),
                            SkScalarToFloat(dest_rect.height()));
  if (resampling == RESAMPLE_AWESOME) {
    paint.setFilterBitmap(false);
    DrawResampledBitmap(*canvas_, paint, bitmap, src_rect, dest_rect);
  } else {
    // No resampling necessary, we can just draw the bitmap.
    // Note: for serialization, we will want to subset the bitmap first so
    // we don't send extra pixels.
    paint.setFilterBitmap(resampling == RESAMPLE_LINEAR);
    canvas_->drawBitmapRect(bitmap, &src_rect, dest_rect, &paint);
  }
}

void SkGraphicsContext::setDashPathEffect(SkDashPathEffect *dash)
{
  paint_context_->setDashPathEffect(dash);
}

void SkGraphicsContext::setGradient(SkShader *gradient)
{
  paint_context_->setGradient(gradient);
}

void SkGraphicsContext::setPattern(SkShader *pattern)
{
  paint_context_->setPattern(pattern);
}

const SkBitmap* SkGraphicsContext::bitmap() const
{
  return &canvas_->getDevice()->accessBitmap(false);
}

gfx::PlatformCanvasWin* SkGraphicsContext::canvas() const
{
  return canvas_;
}

bool SkGraphicsContext::IsPrinting() {
  return canvas_->getTopPlatformDevice().IsVectorial();
}
