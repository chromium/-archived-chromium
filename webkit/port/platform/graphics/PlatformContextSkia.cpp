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
#include "wtf/MathExtras.h"

#include "base/gfx/image_operations.h"
#include "base/gfx/platform_canvas.h"
#include "base/gfx/skia_utils.h"

#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkShader.h"
#include "SkDashPathEffect.h"

#if PLATFORM(WIN_OS)
#include <vssym32.h>
#include "base/gfx/native_theme.h"
#include "ThemeData.h"
#include "UniscribeStateTextRun.h"
#endif

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
                         const SkIRect& srcIRect,
                         const SkRect& destRect)
{
    // First get the subset we need. This is efficient and does not copy pixels.
    SkBitmap subset;
    bitmap.extractSubset(&subset, srcIRect);
    SkRect srcRect;
    srcRect.set(srcIRect);

    // Whether we're doing a subset or using the full source image.
    bool srcIsFull = srcIRect.fLeft == 0 && srcIRect.fTop == 0 &&
        srcIRect.width() == bitmap.width() &&
        srcIRect.height() == bitmap.height();

    // We will always draw in integer sizes, so round the destination rect.
    SkIRect destRectRounded;
    destRect.round(&destRectRounded);
    SkIRect resizedImageRect;  // Represents the size of the resized image.
    resizedImageRect.set(0, 0,
                         destRectRounded.width(), destRectRounded.height());

    if (srcIsFull &&
        bitmap.hasResizedBitmap(destRectRounded.width(),
                                destRectRounded.height())) {
        // Yay, this bitmap frame already has a resized version.
        SkBitmap resampled = bitmap.resizedBitmap(destRectRounded.width(),
                                                  destRectRounded.height());
        canvas.drawBitmapRect(resampled, 0, destRect, &paint);
        return;
    }

    // Compute the visible portion of our rect.
    SkRect destBitmapSubsetSk;
    ClipRectToCanvas(canvas, destRect, &destBitmapSubsetSk);
    destBitmapSubsetSk.offset(-destRect.fLeft, -destRect.fTop);

    // The matrix inverting, etc. could have introduced rounding error which
    // causes the bounds to be outside of the resized bitmap. We round outward
    // so we always lean toward it being larger rather than smaller than we
    // need, and then clamp to the bitmap bounds so we don't get any invalid
    // data.
    SkIRect destBitmapSubsetSkI;
    destBitmapSubsetSk.roundOut(&destBitmapSubsetSkI);
    if (!destBitmapSubsetSkI.intersect(resizedImageRect))
        return;  // Resized image does not intersect.

    if (srcIsFull && bitmap.shouldCacheResampling(
            resizedImageRect.width(),
            resizedImageRect.height(),
            destBitmapSubsetSkI.width(),
            destBitmapSubsetSkI.height())) {
        // We're supposed to resize the entire image and cache it, even though
        // we don't need all of it.
        SkBitmap resampled = bitmap.resizedBitmap(destRectRounded.width(),
                                                  destRectRounded.height());
        canvas.drawBitmapRect(resampled, 0, destRect, &paint);
    } else {
        // We should only resize the exposed part of the bitmap to do the
        // minimal possible work.
        gfx::Rect destBitmapSubset(destBitmapSubsetSkI.fLeft,
                                   destBitmapSubsetSkI.fTop,
                                   destBitmapSubsetSkI.width(),
                                   destBitmapSubsetSkI.height());

        // Resample the needed part of the image.
        SkBitmap resampled = gfx::ImageOperations::Resize(subset,
            gfx::ImageOperations::RESIZE_LANCZOS3,
            gfx::Size(destRectRounded.width(), destRectRounded.height()),
            destBitmapSubset);

        // Compute where the new bitmap should be drawn. Since our new bitmap
        // may be smaller than the original, we have to shift it over by the
        // same amount that we cut off the top and left.
        SkRect offsetDestRect = {
            destBitmapSubset.x() + destRect.fLeft,
            destBitmapSubset.y() + destRect.fTop,
            destBitmapSubset.right() + destRect.fLeft,
            destBitmapSubset.bottom() + destRect.fTop };

        canvas.drawBitmapRect(resampled, 0, offsetDestRect, &paint);
    }
}

}  // namespace

// State -----------------------------------------------------------------------

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
struct PlatformContextSkia::State {
    State();
    State(const State&);
    ~State();

    // Common shader state.
    float m_alpha;
    SkPorterDuff::Mode m_porterDuffMode;
    SkShader* m_gradient;
    SkShader* m_pattern;
    bool m_useAntialiasing;
    SkDrawLooper* m_looper;

    // Fill.
    SkColor m_fillColor;

    // Stroke.
    WebCore::StrokeStyle m_strokeStyle;
    SkColor m_strokeColor;
    float m_strokeThickness;
    int m_dashRatio;  // Ratio of the length of a dash to its width.
    float m_miterLimit;
    SkPaint::Cap m_lineCap;
    SkPaint::Join m_lineJoin;
    SkDashPathEffect* m_dash;

    // Text. (See cTextFill & friends in GraphicsContext.h.)
    int m_textDrawingMode;

    // Helper function for applying the state's alpha value to the given input
    // color to produce a new output color.
    SkColor applyAlpha(SkColor c) const;

private:
    // Not supported.
    void operator=(const State&);
};

// Note: Keep theses default values in sync with GraphicsContextState.
PlatformContextSkia::State::State()
    : m_miterLimit(4)
    , m_alpha(1)
    , m_looper(0)
    , m_lineCap(SkPaint::kDefault_Cap)
    , m_lineJoin(SkPaint::kDefault_Join)
    , m_porterDuffMode(SkPorterDuff::kSrcOver_Mode)
    , m_dashRatio(3)
    , m_fillColor(0xFF000000)
    , m_strokeStyle(WebCore::SolidStroke)
    , m_strokeColor(0x0FF000000)
    , m_strokeThickness(0)
    , m_textDrawingMode(WebCore::cTextFill)
    , m_useAntialiasing(true)
    , m_dash(0)
    , m_gradient(0)
    , m_pattern(0)
{
}

PlatformContextSkia::State::State(const State& other)
{
    other.m_looper->safeRef();
    memcpy(this, &other, sizeof(State));

    m_dash->safeRef();
    m_gradient->safeRef();
    m_pattern->safeRef();
}

PlatformContextSkia::State::~State()
{
    m_looper->safeUnref();
    m_dash->safeUnref();
    m_gradient->safeUnref();
    m_pattern->safeUnref();
}

SkColor PlatformContextSkia::State::applyAlpha(SkColor c) const
{
    int s = roundf(m_alpha * 256);
    if (s >= 256)
        return c;
    if (s < 0)
        return 0;

    int a = SkAlphaMul(SkColorGetA(c), s);
    return (c & 0x00FFFFFF) | (a << 24);
}

// PlatformContextSkia ---------------------------------------------------------

PlatformContextSkia::PlatformContextSkia(gfx::PlatformCanvas* canvas)
    : m_canvas(canvas)
    , m_stateStack(sizeof(State))
{
    State* state = reinterpret_cast<State*>(m_stateStack.push_back());
    new (state) State();
    m_state = state;
}

PlatformContextSkia::~PlatformContextSkia()
{
    // We force restores so we don't leak any subobjects owned by our
    // stack of State records.
    while (m_stateStack.count() > 0) {
        reinterpret_cast<State*>(m_stateStack.back())->~State();
        m_stateStack.pop_back();
    }
}

void PlatformContextSkia::save()
{
    State* newState = reinterpret_cast<State*>(m_stateStack.push_back());
    new (newState) State(*m_state);
    m_state = newState;

    // Save our native canvas.
    canvas()->save();
}

void PlatformContextSkia::restore()
{
    // Restore our native canvas.
    canvas()->restore();

    m_state->~State();
    m_stateStack.pop_back();

    m_state = reinterpret_cast<State*>(m_stateStack.back());
}

void PlatformContextSkia::drawRect(SkRect rect)
{
    SkPaint paint;
    int fillcolorNotTransparent = m_state->m_fillColor & 0xFF000000;
    if (fillcolorNotTransparent) {
        setupPaintForFilling(&paint);
        canvas()->drawRect(rect, paint);
    }

    if (m_state->m_strokeStyle != WebCore::NoStroke &&
        (m_state->m_strokeColor & 0xFF000000)) {
        if (fillcolorNotTransparent) {
            // This call is expensive so don't call it unnecessarily.
            paint.reset();
        }
        setupPaintForStroking(&paint, &rect, 0);
        canvas()->drawRect(rect, paint);
    }
}

void PlatformContextSkia::setupPaintCommon(SkPaint* paint) const
{
#ifdef SK_DEBUGx
    {
        SkPaint defaultPaint;
        SkASSERT(*paint == defaultPaint);
    }
#endif

    paint->setAntiAlias(m_state->m_useAntialiasing);
    paint->setPorterDuffXfermode(m_state->m_porterDuffMode);
    paint->setLooper(m_state->m_looper);

    if (m_state->m_gradient)
        paint->setShader(m_state->m_gradient);
    else if (m_state->m_pattern)
        paint->setShader(m_state->m_pattern);
}

void PlatformContextSkia::setupPaintForFilling(SkPaint* paint) const
{
    setupPaintCommon(paint);
    paint->setColor(m_state->applyAlpha(m_state->m_fillColor));
}

float PlatformContextSkia::setupPaintForStroking(SkPaint* paint,
                                                 SkRect* rect,
                                                 int length) const
{
    setupPaintCommon(paint);
    float width = m_state->m_strokeThickness;

    // This allows dashing and dotting to work properly for hairline strokes.
    if (width == 0)
        width = 1;

    paint->setColor(m_state->applyAlpha(m_state->m_strokeColor));
    paint->setStyle(SkPaint::kStroke_Style);
    paint->setStrokeWidth(SkFloatToScalar(width));
    paint->setStrokeCap(m_state->m_lineCap);
    paint->setStrokeJoin(m_state->m_lineJoin);
    paint->setStrokeMiter(SkFloatToScalar(m_state->m_miterLimit));

    if (rect != 0 && (static_cast<int>(roundf(width)) & 1))
        rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);

    if (m_state->m_dash) {
        paint->setPathEffect(m_state->m_dash);
    } else {
        switch (m_state->m_strokeStyle) {
        case WebCore::NoStroke:
        case WebCore::SolidStroke:
            break;
        case WebCore::DashedStroke:
            width = m_state->m_dashRatio * width;
            // Fall through.
        case WebCore::DottedStroke:
            SkScalar dashLength;
            if (length) {
                // Determine about how many dashes or dots we should have.
                int numDashes = length / roundf(width);
                if (!(numDashes & 1))
                    numDashes++;    // Make it odd so we end on a dash/dot.
                // Use the number of dashes to determine the length of a
                // dash/dot, which will be approximately width
                dashLength = SkScalarDiv(SkIntToScalar(length),
                                         SkIntToScalar(numDashes));
            } else {
                dashLength = SkFloatToScalar(width);
            }
            SkScalar intervals[2] = { dashLength, dashLength };
            paint->setPathEffect(
                new SkDashPathEffect(intervals, 2, 0))->unref();
        }
    }
    return width;
}

void PlatformContextSkia::setDrawLooper(SkDrawLooper* dl)
{
    SkRefCnt_SafeAssign(m_state->m_looper, dl);
}

void PlatformContextSkia::setMiterLimit(float ml)
{
    m_state->m_miterLimit = ml;
}

void PlatformContextSkia::setAlpha(float alpha)
{
    m_state->m_alpha = alpha;
}

void PlatformContextSkia::setLineCap(SkPaint::Cap lc)
{
    m_state->m_lineCap = lc;
}

void PlatformContextSkia::setLineJoin(SkPaint::Join lj)
{
    m_state->m_lineJoin = lj;
}

void PlatformContextSkia::setPorterDuffMode(SkPorterDuff::Mode pdm)
{
    m_state->m_porterDuffMode = pdm;
}

void PlatformContextSkia::setFillColor(SkColor color)
{
    m_state->m_fillColor = color;
}

WebCore::StrokeStyle PlatformContextSkia::getStrokeStyle() const
{
    return m_state->m_strokeStyle;
}

void PlatformContextSkia::setStrokeStyle(WebCore::StrokeStyle strokestyle)
{
    m_state->m_strokeStyle = strokestyle;
}

void PlatformContextSkia::setStrokeColor(SkColor strokecolor)
{
    m_state->m_strokeColor = strokecolor;
}

float PlatformContextSkia::getStrokeThickness() const
{
    return m_state->m_strokeThickness;
}

void PlatformContextSkia::setStrokeThickness(float thickness)
{
    m_state->m_strokeThickness = thickness;
}

int PlatformContextSkia::getTextDrawingMode() const
{
    return m_state->m_textDrawingMode;
}

void PlatformContextSkia::setTextDrawingMode(int mode)
{
  // cTextClip is never used, so we assert that it isn't set:
  // https://bugs.webkit.org/show_bug.cgi?id=21898
  ASSERT((mode & WebCore::cTextClip) == 0);
  m_state->m_textDrawingMode = mode;
}

void PlatformContextSkia::setUseAntialiasing(bool enable)
{
    m_state->m_useAntialiasing = enable;
}

SkColor PlatformContextSkia::fillColor() const
{
    return m_state->m_fillColor;
}

void PlatformContextSkia::beginPath()
{
    m_path.reset();
}

void PlatformContextSkia::addPath(const SkPath& path)
{
    m_path.addPath(path);
}

void PlatformContextSkia::setFillRule(SkPath::FillType fr)
{
    m_path.setFillType(fr);
}

void PlatformContextSkia::setGradient(SkShader* gradient)
{
    if (gradient != m_state->m_gradient) {
        m_state->m_gradient->safeUnref();
        m_state->m_gradient = gradient;
    }
}

void PlatformContextSkia::setPattern(SkShader* pattern)
{
    if (pattern != m_state->m_pattern) {
        m_state->m_pattern->safeUnref();
        m_state->m_pattern = pattern;
    }
}

void PlatformContextSkia::setDashPathEffect(SkDashPathEffect* dash)
{
    if (dash != m_state->m_dash) {
        m_state->m_dash->safeUnref();
        m_state->m_dash = dash;
    }
}

// TODO(brettw) all this platform stuff should be moved out of this class into
// platform-specific files for that type of thing (e.g. to FontWin).
#if PLATFORM(WIN_OS)

const gfx::NativeTheme* PlatformContextSkia::nativeTheme()
{
    return gfx::NativeTheme::instance();
}

// TODO(brettw) move to a platform-specific file.
void PlatformContextSkia::paintIcon(HICON icon, const SkIRect& rect)
{
    HDC hdc = m_canvas->beginPlatformPaint();
    DrawIconEx(hdc, rect.fLeft, rect.fTop, icon, rect.width(), rect.height(),
               0, 0, DI_NORMAL);
    m_canvas->endPlatformPaint();
}

// RenderThemeWin.cpp
void PlatformContextSkia::paintButton(const SkIRect& widgetRect,
                                      const ThemeData& themeData)
{
    RECT rect(gfx::SkIRectToRECT(widgetRect));
    HDC hdc = m_canvas->beginPlatformPaint();
    int state = themeData.m_state;
    nativeTheme()->PaintButton(hdc,
                               themeData.m_part,
                               state,
                               themeData.m_classicState,
                               &rect);
    m_canvas->endPlatformPaint();
}

void PlatformContextSkia::paintTextField(const SkIRect& widgetRect,
                                         const ThemeData& themeData,
                                         SkColor c, 
                                         bool drawEdges)
{
    RECT rect(gfx::SkIRectToRECT(widgetRect));
    HDC hdc = m_canvas->beginPlatformPaint();
    nativeTheme()->PaintTextField(hdc,
                                  themeData.m_part,
                                  themeData.m_state,
                                  themeData.m_classicState,
                                  &rect,
                                  gfx::SkColorToCOLORREF(c),
                                  true,
                                  drawEdges);
    m_canvas->endPlatformPaint();
}

void PlatformContextSkia::paintMenuListArrowButton(const SkIRect& widgetRect,
                                                   unsigned state,
                                                   unsigned classicState)
{
    RECT rect(gfx::SkIRectToRECT(widgetRect));
    HDC hdc = m_canvas->beginPlatformPaint();
    nativeTheme()->PaintMenuList(hdc,
                                 CP_DROPDOWNBUTTON,
                                 state,
                                 classicState,
                                 &rect);
    m_canvas->endPlatformPaint();
}

void PlatformContextSkia::paintComplexText(UniscribeStateTextRun& state,
                                           const SkPoint& point,
                                           int from,
                                           int to,
                                           int ascent)
{
    SkColor color(fillColor());
    uint8 alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha)
        return;

    HDC hdc = m_canvas->beginPlatformPaint();

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
    m_canvas->endPlatformPaint();
}

// TODO(brettw) move to FontWin
bool PlatformContextSkia::paintText(FontHandle hfont,
                                    int numberGlyph,
                                    const uint16* glyphs,
                                    const int* advances,
                                    const SkPoint& origin)
{
    SkColor color(fillColor());
    uint8 alpha = SkColorGetA(color);
    // Skip 100% transparent text; no need to draw anything.
    if (!alpha)
        return true;

    bool success = false;
    HDC hdc = m_canvas->beginPlatformPaint();
    HGDIOBJ oldFont = SelectObject(hdc, hfont);

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
                           ETO_GLYPH_INDEX, 0,
                           reinterpret_cast<const wchar_t*>(glyphs),
                           numberGlyph,
                           advances);
    SelectObject(hdc, oldFont);
    m_canvas->endPlatformPaint();
    return success;
}

#endif  // PLATFORM(WIN_OS);

void PlatformContextSkia::paintSkPaint(const SkRect& rect,
                                       const SkPaint& paint)
{
    m_canvas->drawRect(rect, paint);
}

// static
PlatformContextSkia::ResamplingMode PlatformContextSkia::computeResamplingMode(
    const NativeImageSkia& bitmap,
    int srcWidth, int srcHeight,
    float destWidth, float destHeight)
{
    int destIWidth = static_cast<int>(destWidth);
    int destIHeight = static_cast<int>(destHeight);

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
    if (srcWidth == destIWidth && srcHeight == destIHeight) {
        // We don't need to resample if the source and destination are the same.
        return RESAMPLE_NONE;
    }
    
    if (srcWidth <= kSmallImageSizeThreshold ||
        srcHeight <= kSmallImageSizeThreshold ||
        destWidth <= kSmallImageSizeThreshold ||
        destHeight <= kSmallImageSizeThreshold) {
        // Never resample small images. These are often used for borders and
        // rules (think 1x1 images used to make lines).
        return RESAMPLE_NONE;
    }
    
    if (srcHeight * kLargeStretch <= destHeight ||
        srcWidth * kLargeStretch <= destWidth) {
        // Large image detected.

        // Don't resample if it is being stretched a lot in only one direction.
        // This is trying to catch cases where somebody has created a border
        // (which might be large) and then is stretching it to fill some part
        // of the page.
        if (srcWidth == destWidth || srcHeight == destHeight)
            return RESAMPLE_NONE;

        // The image is growing a lot and in more than one direction. Resampling
        // is slow and doesn't give us very much when growing a lot.
        return RESAMPLE_LINEAR;
    }
    
    if ((fabs(destWidth - srcWidth) / srcWidth <
         kFractionalChangeThreshold) &&
       (fabs(destHeight - srcHeight) / srcHeight <
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
                                        const SkIRect& srcRect,
                                        const SkRect& destRect,
                                        const SkPorterDuff::Mode& compOp)
{
    SkPaint paint;
    paint.setPorterDuffXfermode(compOp);

    ResamplingMode resampling = IsPrinting() ? RESAMPLE_NONE :
        computeResamplingMode(bitmap, srcRect.width(), srcRect.height(),
                              SkScalarToFloat(destRect.width()),
                              SkScalarToFloat(destRect.height()));
    if (resampling == RESAMPLE_AWESOME) {
        paint.setFilterBitmap(false);
        DrawResampledBitmap(*m_canvas, paint, bitmap, srcRect, destRect);
    } else {
        // No resampling necessary, we can just draw the bitmap.
        // Note: for serialization, we will want to subset the bitmap first so
        // we don't send extra pixels.
        paint.setFilterBitmap(resampling == RESAMPLE_LINEAR);
        m_canvas->drawBitmapRect(bitmap, &srcRect, destRect, &paint);
    }
}

const SkBitmap* PlatformContextSkia::bitmap() const
{
    return &m_canvas->getDevice()->accessBitmap(false);
}

bool PlatformContextSkia::IsPrinting()
{
    return m_canvas->getTopPlatformDevice().IsVectorial();
}
