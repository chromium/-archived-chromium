// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "SkGraphicsContext.h"

#include "base/gfx/platform_canvas_mac.h"
#include "base/gfx/image_operations.h"
#include "base/gfx/skia_utils_mac.h"
#include "base/logging.h"
#include "GraphicsContextPlatformPrivate.h"
#include "SkBitmap.h"
#include "NativeImageSkia.h"
#include "SkiaUtils.h"

// These need to be moved
#include "ThemeData.h"

#define kSkiaOrientation  kHIThemeOrientationNormal

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

SkGraphicsContext::SkGraphicsContext(gfx::PlatformCanvas* canvas)
    : canvas_(canvas),
      paint_context_(NULL),
      own_canvas_(false) {
}

SkGraphicsContext::~SkGraphicsContext() {
  if (own_canvas_)
    delete canvas_;
}

const gfx::NativeTheme* SkGraphicsContext::nativeTheme() {
  // TODO(pinkerton): fix me
  return nil;
}

void SkGraphicsContext::paintIcon(CGImageRef icon, const SkIRect& rect) {
  CGContextRef context = canvas_->beginPlatformPaint();
  CGRect r = gfx::SkIRectToCGRect(rect);
  CGContextDrawImage(context, r, icon);
  canvas_->endPlatformPaint();
}


void SkGraphicsContext::paintButton(const SkIRect& widgetRect,
                                    const ThemeData& themeData) {
  CGContextRef context = canvas_->beginPlatformPaint();

  int state = themeData.m_state;
  CGRect rect(gfx::SkIRectToCGRect(widgetRect));
  HIThemeButtonDrawInfo button_draw_info = { 0 };
  button_draw_info.state = state;
  CGRect label_rect;

  HIThemeDrawButton(&rect, &button_draw_info, context,
                    kSkiaOrientation, &label_rect);

  canvas_->endPlatformPaint();
}

void SkGraphicsContext::paintTextField(const SkIRect& widgetRect,
                                       const ThemeData& themeData,
                                       SkColor c,
                                       bool drawEdges) {
  CGContextRef context = canvas_->beginPlatformPaint();

  int state = themeData.m_state;
  CGRect rect(gfx::SkIRectToCGRect(widgetRect));
  CGContextSaveGState(context);
  CGColorRef color = gfx::SkColorToCGColorRef(c);
  CGContextSetFillColorWithColor(context, color);
  CGContextFillRect(context, rect);
  CGContextSetRGBStrokeColor(context, 0.0, 0.0, 0.0, 1.0);  // black border for now
  CGContextStrokeRect(context, rect);
  CGContextRestoreGState(context);
  CGColorRelease(color);
  canvas_->endPlatformPaint();
}

void SkGraphicsContext::paintMenuListArrowButton(const SkIRect& widgetRect,
                                                 unsigned state,
                                                 unsigned classic_state) {
  CGContextRef context = canvas_->beginPlatformPaint();

  CGRect rect(gfx::SkIRectToCGRect(widgetRect));
  HIThemePopupArrowDrawInfo arrow_draw_info = { 0 };
  arrow_draw_info.state = state;
  CGRect label_rect;
  
  HIThemeDrawPopupArrow(&rect, &arrow_draw_info, context, kSkiaOrientation);

  canvas_->endPlatformPaint();
}

void SkGraphicsContext::paintComplexText(UniscribeStateTextRun& state,
                                         const SkPoint& point,
                                         int from,
                                         int to,
                                         int ascent) {
  SkColor sk_color(paint_context_->fillColor());
  uint8 alpha = SkColorGetA(sk_color);
  // Skip 100% transparent text; no need to draw anything.
  if (!alpha)
    return;

  CGContextRef context = canvas_->beginPlatformPaint();
  CGContextSaveGState(context);
  CGColorRef color = gfx::SkColorToCGColorRef(sk_color);
  CGContextSetFillColorWithColor(context, color);

  //TODO: remove use of UniscribeStateTextRun; show placeholder to test other
  // code for the moment
  CGContextShowTextAtPoint(context, point.fX, point.fY, "complex", 7);
  CGContextRestoreGState(context);
  CGColorRelease(color);
  canvas_->endPlatformPaint();
}

bool SkGraphicsContext::paintText(FontHandle font,
                                  int number_glyph,
                                  const uint16* chars,
                                  const int* advances,
                                  const SkPoint& origin) {
  SkColor sk_color(paint_context_->fillColor());
  uint8 alpha = SkColorGetA(sk_color);
  // Skip 100% transparent text; no need to draw anything.
  if (!alpha)
    return true;

  bool success = false;
  CGContextRef context = canvas_->beginPlatformPaint();
  CGContextSaveGState(context);
  CGColorRef color = gfx::SkColorToCGColorRef(sk_color);
  CGContextSetFillColorWithColor(context, color);
  CGContextMoveToPoint(context, origin.fX, origin.fY);
  CGFontRef cg_font = CTFontCopyGraphicsFont(font, NULL);
  CGContextSetFont(context, cg_font);

  CGSize cg_advances[number_glyph];
  CGGlyph cg_glyphs[number_glyph];
 
  CTFontGetGlyphsForCharacters(font, chars, cg_glyphs, number_glyph);
  CGContextShowGlyphsAtPoint(context, origin.fX, origin.fY,
                             cg_glyphs, number_glyph);
  CGContextRestoreGState(context);
  CGColorRelease(color);
  CGFontRelease(cg_font);
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

void SkGraphicsContext::setDashPathEffect(SkDashPathEffect *dash) {
  paint_context_->setDashPathEffect(dash);
}

void SkGraphicsContext::setGradient(SkShader *gradient) {
  paint_context_->setGradient(gradient);
}

void SkGraphicsContext::setPattern(SkShader *pattern) {
  paint_context_->setPattern(pattern);
}

const SkBitmap* SkGraphicsContext::bitmap() const {
  return &canvas_->getDevice()->accessBitmap(false);
}

gfx::PlatformCanvas* SkGraphicsContext::canvas() const {
  return canvas_;
}

bool SkGraphicsContext::IsPrinting() {
  return canvas_->getTopPlatformDevice().IsVectorial();
}
