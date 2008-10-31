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

#ifndef PlatformContextSkia_h
#define PlatformContextSkia_h

#include "base/gfx/platform_canvas.h"

#include "SkDashPathEffect.h"
#include "SkDrawLooper.h"
#include "SkDeque.h"
#include "SkPaint.h"
#include "SkPath.h"

#include "GraphicsContext.h"
#include "ThemeData.h"

#if defined(OS_WIN)
typedef HICON IconHandle;
typedef HFONT FontHandle;
#elif defined(OS_MACOSX)
typedef CGImageRef IconHandle;
typedef CTFontRef FontHandle;
#elif defined(OS_LINUX)
// TODO(erg): Type needs to be defined for half the rest of the stack to
// compile. When the corresponding implementation to this file gets written,
// these void pointers need to be replaced with whatever we end up using.
typedef void* IconHandle;
typedef void* FontHandle;
#endif

class UniscribeStateTextRun;

namespace gfx {
class NativeTheme;
}

// This class holds the platform-specific state for GraphicsContext. We put
// most of our Skia wrappers on this class. In theory, a lot of this stuff could
// be moved to GraphicsContext directly, except that some code external to this
// would like to poke at our graphics layer as well (like the Image and Font
// stuff, which needs some amount of our wrappers and state around SkCanvas).
//
// So in general, this class uses just Skia types except when there's no easy
// conversion. GraphicsContext is responsible for converting the WebKit types to
// Skia types and setting up the eventual call to the Skia functions.
//
// This class then keeps track of all the current Skia state. WebKit expects
// that the graphics state that is pushed and popped by save() and restore()
// includes things like colors and pen styles. Skia does this differently, where
// push and pop only includes transforms and bitmaps, and the application is
// responsible for managing the painting state which is store in separate
// SkPaint objects. This class provides the adaptor that allows the painting
// state to be pushed and popped along with the bitmap.
class PlatformContextSkia {
public:
    // Used by computeResamplingMode to tell how bitmaps should be resampled.
    enum ResamplingMode {
        // Nearest neighbor resampling. Used when we detect that the page is
        // trying to make a pattern by stretching a small bitmap very large.
        RESAMPLE_NONE,

        // Default skia resampling. Used for large growing of images where high
        // quality resampling doesn't get us very much except a slowdown.
        RESAMPLE_LINEAR,

        // High quality resampling.
        RESAMPLE_AWESOME,
    };

    // For printing, there shouldn't be any canvas. canvas can be NULL.
    PlatformContextSkia(gfx::PlatformCanvas* canvas);
    ~PlatformContextSkia();

    void save();

    void restore();

    // Sets up the common flags on a paint for antialiasing, effects, etc.
    // This is implicitly called by setupPaintFill and setupPaintStroke, but
    // you may wish to call it directly sometimes if you don't want that other
    // behavior.
    void setupPaintCommon(SkPaint* paint) const;

    // Sets up the paint for the current fill style.
    void setupPaintForFilling(SkPaint* paint) const;

    // Sets up the paint for stroking. Returns an int representing the width of
    // the pen, or 1 if the pen's width is 0 if a non-zero length is provided,
    // the number of dashes/dots on a dashed/dotted line will be adjusted to
    // start and end that length with a dash/dot.
    float setupPaintForStroking(SkPaint* paint, SkRect* rect, int length) const;

    // State setting functions.
    void setDrawLooper(SkDrawLooper* dl);  // Note: takes an additional ref.
    void setMiterLimit(float ml);
    void setAlpha(float alpha);
    void setLineCap(SkPaint::Cap lc);
    void setLineJoin(SkPaint::Join lj);
    void setFillRule(SkPath::FillType fr);
    void setPorterDuffMode(SkPorterDuff::Mode pdm);
    void setFillColor(SkColor color);
    void setStrokeStyle(WebCore::StrokeStyle strokestyle);
    void setStrokeColor(SkColor strokecolor);
    void setStrokeThickness(float thickness);
    void setTextDrawingMode(int mode);
    void setUseAntialiasing(bool enable);
    void setGradient(SkShader*);
    void setPattern(SkShader*);
    void setDashPathEffect(SkDashPathEffect*);

    WebCore::StrokeStyle getStrokeStyle() const;
    float getStrokeThickness() const;
    int getTextDrawingMode() const;

    // Paths.
    void beginPath();
    void addPath(const SkPath& path);
    const SkPath* currentPath() const { return &m_path; }

    SkColor fillColor() const;

    gfx::PlatformCanvas* canvas() { return m_canvas; }

    // TODO(brettw) why is this in this class?
    void drawRect(SkRect rect);

    // Gets the default theme.
    static const gfx::NativeTheme* nativeTheme();

    void paintIcon(IconHandle icon, const SkIRect& rect);
    // TODO(brettw) these functions should not have to take GraphicsContext
    // pointers! This is cureently required because our theme drawing code
    // requires GraphicsContexts, but this class doesn't know about it. This
    // class's purpose seems no longer necessary, so everything can be moved to
    // GraphicsContext or ThemeWin, and these parameters can be removed.
    void paintButton(const SkIRect& widgetRect,
                     const ThemeData& themeData);
    void paintTextField(const SkIRect& widgetRect,
                        const ThemeData& themeData,
                        SkColor c,
                        bool drawEdges);
    void paintMenuListArrowButton(const SkIRect& widgetRect,
                                  unsigned state,
                                  unsigned classicState);
    void paintComplexText(UniscribeStateTextRun& state,
                          const SkPoint& point,
                          int from,
                          int to,
                          int ascent);

    // Draws the given glyphs at the given |origin|. |origin| is on the baseline
    // of the text, and the |ascender_height| must be given for some code paths
    // since Windows draws text from the upper left.
    bool paintText(FontHandle hfont,
                   int number_glyph,
                   const uint16* glyphs,
                   const int* advances,
                   const SkPoint& origin);

    // TODO(maruel): I'm still unsure how I will serialize this call.
    void paintSkPaint(const SkRect& rect, const SkPaint& paint);

    // Draws the given bitmap in the canvas at the location specified in
    // |dest_rect|. It will be resampled as necessary to fill that rectangle.
    // The |src_rect| indicates the subset of the bitmap to draw.
    void paintSkBitmap(const NativeImageSkia& bitmap,
                       const SkIRect& srcRect,
                       const SkRect& destRect,
                       const SkPorterDuff::Mode& compositeOp);

    // Returns true if the given bitmap subset should be resampled before being
    // painted into a rectangle of the given size. This is used to indicate
    // whether bitmap painting should be optimized by not resampling, or given
    // higher quality by resampling.
    static ResamplingMode computeResamplingMode(const NativeImageSkia& bitmap,
                                                int srcWidth,
                                                int srcHeight,
                                                float destWidth,
                                                float destHeight);

    const SkBitmap* bitmap() const;

    // Returns the canvas used for painting, NOT guaranteed to be non-NULL.
    //
    // Warning: This function is deprecated so the users are reminded that they
    // should use this layer of indirection instead of using the canvas
    // directly. This is to help with the eventual serialization.
    gfx::PlatformCanvas* canvas() const;

    // Returns if the context is a printing context instead of a display
    // context. Bitmap shouldn't be resampled when printing to keep the best
    // possible quality.
    bool IsPrinting();

private:
    // Defines drawing style.
    struct State;

    // NULL indicates painting is disabled. Never delete this object.
    gfx::PlatformCanvas* m_canvas;

    // States stack. Enables local drawing state change with save()/restore()
    // calls.
    SkDeque m_stateStack;
    // Pointer to the current drawing state. This is a cached value of
    // mStateStack.back().
    State* m_state;

    // Current path.
    SkPath m_path;

    // Disallow these.
    PlatformContextSkia(const PlatformContextSkia&);
    void operator=(const PlatformContextSkia&);
};

#endif  // PlatformContextSkia_h
