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

#ifndef CHROME_COMMON_GFX_CHROME_CANVAS_H__
#define CHROME_COMMON_GFX_CHROME_CANVAS_H__

#include <windows.h>
#include <string>
#include "base/basictypes.h"
#include "base/gfx/platform_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"

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
class ChromeCanvas : public gfx::PlatformCanvas {
 public:
  // Specifies the alignment for text rendered with the DrawStringInt method.
  static const int TEXT_ALIGN_LEFT = 1;
  static const int TEXT_ALIGN_CENTER = 2;
  static const int TEXT_ALIGN_RIGHT = 4;
  static const int TEXT_VALIGN_TOP = 8;
  static const int TEXT_VALIGN_MIDDLE = 16;
  static const int TEXT_VALIGN_BOTTOM = 32;

  // Specifies the text consists of multiple lines.
  static const int MULTI_LINE = 64;

  // By default DrawStringInt does not process the prefix ('&') character
  // specially. That is, the string "&foo" is rendered as "&foo". When
  // rendering text from a resource that uses the prefix character for
  // mnemonics, the prefix should be processed and can be rendered as an
  // underline (SHOW_PREFIX), or not rendered at all (HIDE_PREFIX).
  static const int SHOW_PREFIX = 128;
  static const int HIDE_PREFIX = 256;

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
                     const SkColor& color, int x, int y, int w, int h) {
    DrawStringInt(text, font, color, x, y, w, h,
                  l10n_util::DefaultCanvasTextAlignment());
  }

  // Draws text with the specified color, font and location. The last argument
  // specifies flags for how the text should be rendered. It can be one of
  // TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT or TEXT_ALIGN_LEFT.
  void DrawStringInt(const std::wstring& text, const ChromeFont& font,
                     const SkColor& color, int x, int y, int w, int h,
                     int flags);

  // Draws a dotted gray rectangle used for focus purposes.
  void DrawFocusRect(int x, int y, int width, int height);

  // Compute the size required to draw some text with the provided font.
  // Attempts to fit the text with the provided width and height. Increases
  // height and then width as needed to make the text fit. This method
  // supports multiple lines.
  void SizeStringInt(const std::wstring& test, const ChromeFont& font,
                     int *width, int* height, int flags);

  // Tiles the image in the specified region.
  void TileImageInt(const SkBitmap& bitmap, int x, int y, int w, int h,
                    SkPorterDuff::Mode mode);

  // Tiles the image in the specified region using a transfer mode of
  // SkPorterDuff::kSrcOver_Mode.
  void TileImageInt(const SkBitmap& bitmap, int x, int y, int w, int h);

  // Extracts a bitmap from the contents of this canvas.
  SkBitmap ExtractBitmap();

 private:
  // Draws text with the specified color, font and location. The text is
   // aligned to the left, vertically centered, clipped to the region. If the
   // text is too big, it is truncated and '...' is added to the end.
  void DrawStringInt(const std::wstring& text, HFONT font,
                     const SkColor& color, int x, int y, int w, int h,
                     int flags);

  // Compute the windows flags necessary to implement the provided text
  // ChromeCanvas flags
  int ChromeCanvas::ComputeFormatFlags(int flags);

  // A wrapper around Windows' DrawText. This function takes care of adding
  // Unicode directionality marks to the text in certain cases.
  void DoDrawText(HDC hdc, const std::wstring& text, RECT* text_bounds,
                  int flags);

  DISALLOW_EVIL_CONSTRUCTORS(ChromeCanvas);
};

typedef gfx::CanvasPaintT<ChromeCanvas> ChromeCanvasPaint;

#endif  // CHROME_COMMON_GFX_CHROME_CANVAS_H__
