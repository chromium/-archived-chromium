// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkGraphicsContext_h
#define SkGraphicsContext_h

#include "SkPorterDuff.h"

#include "base/gfx/platform_canvas.h"

class NativeImageSkia;
struct SkIRect;
struct SkPoint;
struct SkRect;
struct ThemeData;
class SkBitmap;
class SkCanvas;
class SkPaint;
class SkPaintContext;
class SkDashPathEffect;
class SkShader;
class UniscribeStateTextRun;

namespace gfx {
class NativeTheme;
}

#if defined(OS_WIN)
typedef HICON IconHandle;
typedef HFONT FontHandle;
#elif defined(OS_MACOSX)
typedef CGImageRef IconHandle;
typedef CTFontRef FontHandle;
#endif

class SkGraphicsContext {
 public:
  // Used by computeResamplingMode to indicate how bitmaps should be resampled.
  enum ResamplingMode {
    // Nearest neighbor resampling. Used when we detect that the page is trying
    // to make a pattern by stretching a small bitmap very large.
    RESAMPLE_NONE,

    // Default skia resampling. Used for large growing of images where high
    // quality resampling doesn't get us very much except a slowdown.
    RESAMPLE_LINEAR,

    // High quality resampling.
    RESAMPLE_AWESOME,
  };

  SkGraphicsContext(gfx::PlatformCanvas* canvas);
  ~SkGraphicsContext();

  // Gets the default theme.
  static const gfx::NativeTheme* nativeTheme();

  void paintIcon(IconHandle icon, const SkIRect& rect);
  void paintButton(const SkIRect& widgetRect,
                   const ThemeData& themeData);
  void paintTextField(const SkIRect& widgetRect,
                      const ThemeData& themeData,
                      SkColor c,
                      bool drawEdges);
  void paintMenuListArrowButton(const SkIRect& widgetRect,
                                unsigned state,
                                unsigned classic_state);
  void paintComplexText(UniscribeStateTextRun& state,
                        const SkPoint& point,
                        int from,
                        int to,
                        int ascent);
#if defined(OS_WIN)
  bool paintText(FontHandle hfont,
                 int number_glyph,
                 const WORD* glyphs,
                 const int* advances,
                 const SkPoint& origin);
#endif

  // TODO(maruel): I'm still unsure how I will serialize this call.
  void paintSkPaint(const SkRect& rect, const SkPaint& paint);

  // Draws the given bitmap in the canvas at the location specified in
  // |dest_rect|. It will be resampled as necessary to fill that rectangle. The
  // |src_rect| indicates the subset of the bitmap to draw.
  void paintSkBitmap(const NativeImageSkia& bitmap,
                     const SkIRect& src_rect,
                     const SkRect& dest_rect,
                     const SkPorterDuff::Mode& composite_op);

  // Returns true if the given bitmap subset should be resampled before being
  // painted into a rectangle of the given size. This is used to indicate
  // whether bitmap painting should be optimized by not resampling, or given
  // higher quality by resampling.
  static ResamplingMode computeResamplingMode(const NativeImageSkia& bitmap,
                                              int src_width,
                                              int src_height,
                                              float dest_width,
                                              float dest_height);

  void setShouldDelete(bool should_delete) {
    own_canvas_ = should_delete;
  }

  void setDashPathEffect(SkDashPathEffect* dash);
  void setGradient(SkShader* gradient);
  void setPattern(SkShader* gradient);

  const SkBitmap* bitmap() const;

  // Returns the canvas used for painting, NOT guaranteed to be non-NULL.
  //
  // Warning: This function is deprecated so the users are reminded that they
  // should use this layer of indirection instead of using the canvas directly.
  // This is to help with the eventual serialization.
  gfx::PlatformCanvas* canvas() const;

  // Returns if the context is a printing context instead of a display context.
  // Bitmap shouldn't be resampled when printing to keep the best possible
  // quality.
  bool IsPrinting();

 protected:
  void setPaintContext(SkPaintContext* context) {
    paint_context_ = context;
  }

 private:
  // Keep the painting state.
  SkPaintContext* paint_context_;

  // Can be NULL when serializing.
  gfx::PlatformCanvas* canvas_;

  // Signal that we own the canvas and must delete it on destruction.
  bool own_canvas_;

  // Disallow these.
  SkGraphicsContext(const SkGraphicsContext&);
  void operator=(const SkGraphicsContext&);
};

#endif  // SkGraphicsContext_h

