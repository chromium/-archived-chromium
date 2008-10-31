// Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "config.h"
#include "GraphicsContext.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#undef LOG
#include "SkiaUtils.h"

#include "base/gfx/image_operations.h"
#include "base/gfx/native_theme.h"
#include "base/gfx/platform_canvas.h"
#include "base/gfx/skia_utils.h"

#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkShader.h"
#include "SkDashPathEffect.h"

#if defined(OS_WIN)
#include <vssym32.h>
#include "base/gfx/native_theme.h"
#include "ThemeData.h"
#include "UniscribeStateTextRun.h"
#endif

namespace {

int RoundToInt(float x) {
  // Android uses roundf which VC doesn't have, emulate that function.
  if (fmodf(x, 1.0f) >= 0.5f)
    return (int)ceilf(x);
  return (int)floorf(x);
}

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

}

// State -----------------------------------------------------------------------

struct PlatformContextSkia::State {
  float               mMiterLimit;
  float               mAlpha;
  SkDrawLooper*       mLooper;
  SkPaint::Cap        mLineCap;
  SkPaint::Join       mLineJoin;
  SkPorterDuff::Mode  mPorterDuffMode;
  // Ratio of the length of a dash to its width.
  int                 mDashRatio;
  SkColor             mFillColor;
  WebCore::StrokeStyle mStrokeStyle;
  SkColor             mStrokeColor;
  float               mStrokeThickness;
  int                 mTextDrawingMode;  // See GraphicsContext.h
  bool                mUseAntialiasing;

  SkDashPathEffect*   mDash;
  SkShader*           mGradient;
  SkShader*           mPattern;

  // Note: Keep theses default values in sync with GraphicsContextState.
  State()
      : mMiterLimit(4),
        mAlpha(1),
        mLooper(NULL),
        mLineCap(SkPaint::kDefault_Cap),
        mLineJoin(SkPaint::kDefault_Join),
        mPorterDuffMode(SkPorterDuff::kSrcOver_Mode),
        mDashRatio(3),
        mFillColor(0xFF000000),
        mStrokeStyle(WebCore::SolidStroke),
        mStrokeColor(0x0FF000000),
        mStrokeThickness(0),
        mTextDrawingMode(WebCore::cTextFill),
        mUseAntialiasing(true),
        mDash(NULL),
        mGradient(NULL),
        mPattern(NULL) {
  }

  State(const State& other) {
    other.mLooper->safeRef();
    memcpy(this, &other, sizeof(State));

    mDash->safeRef();
    mGradient->safeRef();
    mPattern->safeRef();
  }

  ~State() {
    mLooper->safeUnref();
    mDash->safeUnref();
    mGradient->safeUnref();
    mPattern->safeUnref();
  }

  SkDrawLooper* setDrawLooper(SkDrawLooper* dl) {
    SkRefCnt_SafeAssign(mLooper, dl);
    return dl;
  }

  SkColor applyAlpha(SkColor c) const {
    int s = RoundToInt(mAlpha * 256);
    if (s >= 256)
      return c;           
    if (s < 0)
      return 0;

    int a = SkAlphaMul(SkColorGetA(c), s);
    return (c & 0x00FFFFFF) | (a << 24);
  }

 private:
  // Not supported yet.
  void operator=(const State&);
};

PlatformContextSkia::PlatformContextSkia(gfx::PlatformCanvas* canvas)
    : canvas_(canvas)
    , state_stack_(sizeof(State))
{
    State* state = reinterpret_cast<State*>(state_stack_.push_back());
    new (state) State();
    state_ = state;
}

PlatformContextSkia::~PlatformContextSkia()
{
    // we force restores so we don't leak any subobjects owned by our
    // stack of State records.
    while (state_stack_.count() > 0) {
        reinterpret_cast<State*>(state_stack_.back())->~State();
        state_stack_.pop_back();
    }
}


void PlatformContextSkia::save() {
  State* newState = reinterpret_cast<State*>(state_stack_.push_back());
  new (newState) State(*state_);
  state_ = newState;

  // Save our native canvas.
  canvas()->save();
}

void PlatformContextSkia::restore() {
  // Restore our native canvas.
  canvas()->restore();

  state_->~State();
  state_stack_.pop_back();

  state_ = reinterpret_cast<State*>(state_stack_.back());
}

void PlatformContextSkia::drawRect(SkRect rect) {
  SkPaint paint;
  int fillcolor_not_transparent = state_->mFillColor & 0xFF000000;
  if (fillcolor_not_transparent) {
    setup_paint_fill(&paint);
    canvas()->drawRect(rect, paint);
  }

  if (state_->mStrokeStyle != WebCore::NoStroke &&
      (state_->mStrokeColor & 0xFF000000)) {
    if (fillcolor_not_transparent) {
      // This call is expensive so don't call it unnecessarily.
      paint.reset();
    }
    setup_paint_stroke(&paint, &rect, 0);
    canvas()->drawRect(rect, paint);
  }
}

void PlatformContextSkia::setup_paint_common(SkPaint* paint) const {
#ifdef SK_DEBUGx
  {
    SkPaint defaultPaint;
    SkASSERT(*paint == defaultPaint);
  }
#endif

  paint->setAntiAlias(state_->mUseAntialiasing);
  paint->setPorterDuffXfermode(state_->mPorterDuffMode);
  paint->setLooper(state_->mLooper);

  if(state_->mGradient) {
    paint->setShader(state_->mGradient);
  } else if (state_->mPattern) {
    paint->setShader(state_->mPattern);
  }
}

void PlatformContextSkia::setup_paint_fill(SkPaint* paint) const {
  setup_paint_common(paint);
  paint->setColor(state_->applyAlpha(state_->mFillColor));
}

int PlatformContextSkia::setup_paint_stroke(SkPaint* paint,
                                       SkRect* rect,
                                       int length) const {
  setup_paint_common(paint);
  float width = state_->mStrokeThickness;

  //this allows dashing and dotting to work properly for hairline strokes
  if (0 == width)
    width = 1;

  paint->setColor(state_->applyAlpha(state_->mStrokeColor));
  paint->setStyle(SkPaint::kStroke_Style);
  paint->setStrokeWidth(SkFloatToScalar(width));
  paint->setStrokeCap(state_->mLineCap);
  paint->setStrokeJoin(state_->mLineJoin);
  paint->setStrokeMiter(SkFloatToScalar(state_->mMiterLimit));

  if (rect != NULL && (RoundToInt(width) & 1)) {
    rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);
  }

  if (state_->mDash) {
    paint->setPathEffect(state_->mDash);
  } else {
    switch (state_->mStrokeStyle) {
    case WebCore::NoStroke:
    case WebCore::SolidStroke:
      break;
    case WebCore::DashedStroke:
      width = state_->mDashRatio * width;
      /* no break */
    case WebCore::DottedStroke:
      SkScalar dashLength;
      if (length) {
        //determine about how many dashes or dots we should have
        int numDashes = length/RoundToInt(width);
        if (!(numDashes & 1))
          numDashes++;    //make it odd so we end on a dash/dot
        //use the number of dashes to determine the length of a dash/dot, which will be approximately width
        dashLength = SkScalarDiv(SkIntToScalar(length), SkIntToScalar(numDashes));
      } else {
        dashLength = SkFloatToScalar(width);
      }
      SkScalar intervals[2] = { dashLength, dashLength };
      paint->setPathEffect(new SkDashPathEffect(intervals, 2, 0))->unref();
    }
  }
  return RoundToInt(width);
}

SkDrawLooper* PlatformContextSkia::setDrawLooper(SkDrawLooper* dl) {
  return state_->setDrawLooper(dl);
}

void PlatformContextSkia::setMiterLimit(float ml) {
  state_->mMiterLimit = ml;
}

void PlatformContextSkia::setAlpha(float alpha) {
  state_->mAlpha = alpha;
}

void PlatformContextSkia::setLineCap(SkPaint::Cap lc) {
  state_->mLineCap = lc;
}

void PlatformContextSkia::setLineJoin(SkPaint::Join lj) {
  state_->mLineJoin = lj;
}

void PlatformContextSkia::setPorterDuffMode(SkPorterDuff::Mode pdm) {
  state_->mPorterDuffMode = pdm;
}

void PlatformContextSkia::setFillColor(SkColor color) {
  state_->mFillColor = color;
}

WebCore::StrokeStyle PlatformContextSkia::getStrokeStyle() const {
  return state_->mStrokeStyle;
}

void PlatformContextSkia::setStrokeStyle(WebCore::StrokeStyle strokestyle) {
  state_->mStrokeStyle = strokestyle;
}

void PlatformContextSkia::setStrokeColor(SkColor strokecolor) {
  state_->mStrokeColor = strokecolor;
}

float PlatformContextSkia::getStrokeThickness() const {
  return state_->mStrokeThickness;
}

void PlatformContextSkia::setStrokeThickness(float thickness) {
  state_->mStrokeThickness = thickness;
}

int PlatformContextSkia::getTextDrawingMode() const {
  return state_->mTextDrawingMode;
}

void PlatformContextSkia::setTextDrawingMode(int mode) {
  // cTextClip is never used, so we assert that it isn't set:
  // https://bugs.webkit.org/show_bug.cgi?id=21898
  ASSERT((mode & WebCore::cTextClip) == 0);
  state_->mTextDrawingMode = mode;
}

void PlatformContextSkia::setUseAntialiasing(bool enable) {
  state_->mUseAntialiasing = enable;
}

SkColor PlatformContextSkia::fillColor() const {
  return state_->mFillColor;
}

void PlatformContextSkia::beginPath() {
  path_.reset();
}

void PlatformContextSkia::addPath(const SkPath& path) {
  path_.addPath(path);
}

const SkPath* PlatformContextSkia::currentPath() const {
  return &path_;
}

void PlatformContextSkia::setFillRule(SkPath::FillType fr) {
  path_.setFillType(fr);
}

void PlatformContextSkia::setGradient(SkShader* gradient) {
  if (gradient != state_->mGradient) {
    state_->mGradient->safeUnref();
    state_->mGradient=gradient;
  }
}

void PlatformContextSkia::setPattern(SkShader* pattern) {
  if (pattern != state_->mPattern) {
    state_->mPattern->safeUnref();
    state_->mPattern=pattern;
  }
}

void PlatformContextSkia::setDashPathEffect(SkDashPathEffect* dash) {
  if (dash != state_->mDash) {
    state_->mDash->safeUnref();
    state_->mDash=dash;
  }
}

const gfx::NativeTheme* PlatformContextSkia::nativeTheme() {
  return gfx::NativeTheme::instance();
}

void PlatformContextSkia::paintIcon(HICON icon, const SkIRect& rect) {
  HDC hdc = canvas_->beginPlatformPaint();
  DrawIconEx(hdc, rect.fLeft, rect.fTop, icon, rect.width(), rect.height(),
             0, 0, DI_NORMAL);
  canvas_->endPlatformPaint();
}

// RenderThemeWin.cpp
void PlatformContextSkia::paintButton(const SkIRect& widgetRect,
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

void PlatformContextSkia::paintTextField(const SkIRect& widgetRect,
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

void PlatformContextSkia::paintMenuListArrowButton(const SkIRect& widgetRect,
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

void PlatformContextSkia::paintComplexText(UniscribeStateTextRun& state,
                                         const SkPoint& point,
                                         int from,
                                         int to,
                                         int ascent) {
  SkColor color(fillColor());
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

bool PlatformContextSkia::paintText(FontHandle hfont,
                                    int number_glyph,
                                    const uint16* glyphs,
                                    const int* advances,
                                    const SkPoint& origin) {
  SkColor color(fillColor());
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

void PlatformContextSkia::paintSkPaint(const SkRect& rect,
                                       const SkPaint& paint) {
  canvas_->drawRect(rect, paint);
}

// static
PlatformContextSkia::ResamplingMode PlatformContextSkia::computeResamplingMode(
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

void PlatformContextSkia::paintSkBitmap(const NativeImageSkia& bitmap,
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

const SkBitmap* PlatformContextSkia::bitmap() const
{
  return &canvas_->getDevice()->accessBitmap(false);
}

gfx::PlatformCanvas* PlatformContextSkia::canvas() const
{
  return canvas_;
}

bool PlatformContextSkia::IsPrinting() {
  return canvas_->getTopPlatformDevice().IsVectorial();
}
