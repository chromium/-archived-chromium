// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_CHROME_CANVAS_H_
#define CHROME_COMMON_GFX_CHROME_CANVAS_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/basictypes.h"
#include "skia/ext/platform_canvas.h"

class ChromeFont;
namespace gfx {
class Rect;
}

// ChromeCanvas is the SkCanvas used by Views for all painting. It
// provides a handful of methods for the common operations used throughout
// Views. With few exceptions, you should NOT create a ChromeCanvas directly,
// rather one will be passed to you via the various paint methods in view.
//
// All methods that take integer arguments (as is used throughout views)
// end with Int. If you need to use methods provided by the superclass
// you'll need to do a conversion. In particular you'll need to use
// macro SkIntToScalar(xxx), or if converting from a scalar to an integer
// SkScalarRound.
//
// A handful of methods in this class are overloaded providing an additional
// argument of type SkPorterDuff::Mode. SkPorterDuff::Mode specifies how the
// source and destination colors are combined. Unless otherwise specified,
// the variant that does not take a SkPorterDuff::Mode uses a transfer mode
// of kSrcOver_Mode.
class ChromeCanvas : public skia::PlatformCanvas {
 public:
  // Specifies the alignment for text rendered with the DrawStringInt method.
  enum {
    TEXT_ALIGN_LEFT = 1,
    TEXT_ALIGN_CENTER = 2,
    TEXT_ALIGN_RIGHT = 4,
    TEXT_VALIGN_TOP = 8,
    TEXT_VALIGN_MIDDLE = 16,
    TEXT_VALIGN_BOTTOM = 32,

    // Specifies the text consists of multiple lines.
    MULTI_LINE = 64,

    // By default DrawStringInt does not process the prefix ('&') character
    // specially. That is, the string "&foo" is rendered as "&foo". When
    // rendering text from a resource that uses the prefix character for
    // mnemonics, the prefix should be processed and can be rendered as an
    // underline (SHOW_PREFIX), or not rendered at all (HIDE_PREFIX).
    SHOW_PREFIX = 128,
    HIDE_PREFIX = 256,

    // Prevent ellipsizing
    NO_ELLIPSIS = 512,
  };

  // Creates an empty ChromeCanvas. Callers must use initialize before using
  // the canvas.
  ChromeCanvas();

  ChromeCanvas(int width, int height, bool is_opaque);

  virtual ~ChromeCanvas();

  // Retrieves the clip rectangle and sets it in the specified rectangle if any.
  // Returns true if the clip rect is non-empty.
  bool GetClipRect(gfx::Rect* clip_rect);

  // Wrapper function that takes integer arguments.
  // Returns true if the clip is non-empty.
  // See clipRect for specifics.
  bool ClipRectInt(int x, int y, int w, int h);

  // Test whether the provided rectangle intersects the current clip rect.
  bool IntersectsClipRectInt(int x, int y, int w, int h);

  // Wrapper function that takes integer arguments.
  // See translate() for specifics.
  void TranslateInt(int x, int y);

  // Wrapper function that takes integer arguments.
  // See scale() for specifics.
  void ScaleInt(int x, int y);

  // Fills the given rectangle with the given paint's parameters.
  void FillRectInt(int x, int y, int w, int h, const SkPaint& paint);

  // Fills the specified region with the specified color using a transfer
  // mode of SkPorterDuff::kSrcOver_Mode.
  void FillRectInt(const SkColor& color, int x, int y, int w, int h);

  // Draws a single pixel line in the specified region with the specified
  // color, using a transfer mode of SkPorterDuff::kSrcOver_Mode.
  void DrawRectInt(const SkColor& color, int x, int y, int w, int h);

  // Draws a single pixel line in the specified region with the specified
  // color and transfer mode.
  void DrawRectInt(const SkColor& color, int x, int y, int w, int h,
                   SkPorterDuff::Mode mode);

  // Draws a bitmap with the origin at the specified location. The upper left
  // corner of the bitmap is rendered at the specified location.
  void DrawBitmapInt(const SkBitmap& bitmap, int x, int y);

  // Draws a bitmap with the origin at the specified location, using the
  // specified paint. The upper left corner of the bitmap is rendered at the
  // specified location.
  void DrawBitmapInt(const SkBitmap& bitmap, int x, int y,
                     const SkPaint& paint);

  // Draws a portion of a bitmap in the specified location. The src parameters
  // correspond to the region of the bitmap to draw in the region defined
  // by the dest coordinates.
  //
  // If the width or height of the source differs from that of the destination,
  // the bitmap will be scaled. When scaling down, it is highly recommended
  // that you call buildMipMap(false) on your bitmap to ensure that it has
  // a mipmap, which will result in much higher-quality output. Set |filter|
  // to use filtering for bitmaps, otherwise the nearest-neighbor algorithm
  // is used for resampling.
  //
  // An optional custom SkPaint can be provided.
  void DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y, int src_w,
                     int src_h, int dest_x, int dest_y, int dest_w, int dest_h,
                     bool filter);
  void DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y, int src_w,
                     int src_h, int dest_x, int dest_y, int dest_w, int dest_h,
                     bool filter, const SkPaint& paint);

  // Draws text with the specified color, font and location. The text is
  // aligned to the left, vertically centered, clipped to the region. If the
  // text is too big, it is truncated and '...' is added to the end.
  void DrawStringInt(const std::wstring& text, const ChromeFont& font,
                     const SkColor& color, int x, int y, int w, int h);

  // Draws text with the specified color, font and location. The last argument
  // specifies flags for how the text should be rendered. It can be one of
  // TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT or TEXT_ALIGN_LEFT.
  void DrawStringInt(const std::wstring& text, const ChromeFont& font,
                     const SkColor& color, int x, int y, int w, int h,
                     int flags);

  // Draws a dotted gray rectangle used for focus purposes.
  void DrawFocusRect(int x, int y, int width, int height);

  // Tiles the image in the specified region.
  void TileImageInt(const SkBitmap& bitmap, int x, int y, int w, int h);

  // Extracts a bitmap from the contents of this canvas.
  SkBitmap ExtractBitmap();

  // Compute the size required to draw some text with the provided font.
  // Attempts to fit the text with the provided width and height. Increases
  // height and then width as needed to make the text fit. This method
  // supports multiple lines.
  static void SizeStringInt(const std::wstring& test, const ChromeFont& font,
                            int *width, int* height, int flags);

 private:
#if defined(OS_WIN)
  // Draws text with the specified color, font and location. The text is
  // aligned to the left, vertically centered, clipped to the region. If the
  // text is too big, it is truncated and '...' is added to the end.
  void DrawStringInt(const std::wstring& text, HFONT font,
                     const SkColor& color, int x, int y, int w, int h,
                     int flags);
#endif

  DISALLOW_EVIL_CONSTRUCTORS(ChromeCanvas);
};

#if defined(OS_WIN)
typedef skia::CanvasPaintT<ChromeCanvas> ChromeCanvasPaint;
#endif

#endif  // CHROME_COMMON_GFX_CHROME_CANVAS_H_

