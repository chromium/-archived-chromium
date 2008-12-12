// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_canvas.h"

#include <limits>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "skia/include/SkShader.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"

namespace {

// We make sure that LTR text we draw in an RTL context is modified
// appropriately to make sure it maintains it LTR orientation.
void DoDrawText(HDC hdc, const std::wstring& text,
                RECT* text_bounds, int flags) {
  std::wstring localized_text;
  const wchar_t* string_ptr = text.c_str();
  int string_size = static_cast<int>(text.length());
  if (l10n_util::AdjustStringForLocaleDirection(text, &localized_text)) {
    string_ptr = localized_text.c_str();
    string_size = static_cast<int>(localized_text.length());
  }

  DrawText(hdc, string_ptr, string_size, text_bounds, flags);
}

// Compute the windows flags necessary to implement the provided text
// ChromeCanvas flags.
int ComputeFormatFlags(int flags) {
  int f = 0;

  // Setting the text alignment explicitly in case it hasn't already been set.
  // This will make sure that we don't align text to the left on RTL locales
  // just because no alignment flag was passed to DrawStringInt().
  if (!(flags & (ChromeCanvas::TEXT_ALIGN_CENTER |
                 ChromeCanvas::TEXT_ALIGN_RIGHT |
                 ChromeCanvas::TEXT_ALIGN_LEFT))) {
    flags |= l10n_util::DefaultCanvasTextAlignment();
  }

  if (flags & ChromeCanvas::HIDE_PREFIX)
    f |= DT_HIDEPREFIX;
  else if ((flags & ChromeCanvas::SHOW_PREFIX) == 0)
    f |= DT_NOPREFIX;

  if (flags & ChromeCanvas::MULTI_LINE) {
    f |= DT_WORDBREAK;
  } else {
    f |= DT_SINGLELINE | DT_VCENTER;
    if (!(flags & ChromeCanvas::NO_ELLIPSIS))
      f |= DT_END_ELLIPSIS;
  }

  // vertical alignment
  if (flags & ChromeCanvas::TEXT_VALIGN_TOP)
    f |= DT_TOP;
  else if (flags & ChromeCanvas::TEXT_VALIGN_BOTTOM)
    f |= DT_BOTTOM;
  else
    f |= DT_VCENTER;

  // horizontal alignment
  if (flags & ChromeCanvas::TEXT_ALIGN_CENTER)
    f |= DT_CENTER;
  else if (flags & ChromeCanvas::TEXT_ALIGN_RIGHT)
    f |= DT_RIGHT;
  else
    f |= DT_LEFT;

  // In order to make sure RTL/BiDi strings are rendered correctly, we must
  // pass the flag DT_RTLREADING to DrawText (when the locale's language is
  // a right-to-left language) so that Windows does the right thing.
  //
  // In addition to correctly displaying text containing both RTL and LTR
  // elements (for example, a string containing a telephone number within a
  // sentence in Hebrew, or a sentence in Hebrew that contains a word in
  // English) this flag also makes sure that if there is not enough space to
  // display the entire string, the ellipsis is displayed on the left hand side
  // of the truncated string and not on the right hand side.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    f |= DT_RTLREADING;

  return f;
}

}  // namespace

ChromeCanvas::ChromeCanvas(int width, int height, bool is_opaque)
    : gfx::PlatformCanvasWin(width, height, is_opaque) {
}

ChromeCanvas::ChromeCanvas() : gfx::PlatformCanvasWin() {
}

ChromeCanvas::~ChromeCanvas() {
}

bool ChromeCanvas::GetClipRect(gfx::Rect* r) {
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

bool ChromeCanvas::ClipRectInt(int x, int y, int w, int h) {
  SkRect new_clip;
  new_clip.set(SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(x + w), SkIntToScalar(y + h));
  return clipRect(new_clip);
}

bool ChromeCanvas::IntersectsClipRectInt(int x, int y, int w, int h) {
  SkRect clip;
  return getClipBounds(&clip) &&
      clip.intersect(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(x + w),
                     SkIntToScalar(y + h));
}

void ChromeCanvas::TranslateInt(int x, int y) {
  translate(SkIntToScalar(x), SkIntToScalar(y));
}

void ChromeCanvas::ScaleInt(int x, int y) {
  scale(SkIntToScalar(x), SkIntToScalar(y));
}

void ChromeCanvas::FillRectInt(const SkColor& color,
                               int x, int y, int w, int h) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setPorterDuffXfermode(SkPorterDuff::kSrcOver_Mode);
  FillRectInt(x, y, w, h, paint);
}

void ChromeCanvas::FillRectInt(int x, int y, int w, int h,
                               const SkPaint& paint) {
  SkRect rc = {SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(x + w), SkIntToScalar(y + h) };
  drawRect(rc, paint);
}

void ChromeCanvas::DrawRectInt(const SkColor& color,
                               int x, int y, int w, int h) {
  DrawRectInt(color, x, y, w, h, SkPorterDuff::kSrcOver_Mode);
}

void ChromeCanvas::DrawRectInt(const SkColor& color, int x, int y, int w, int h,
                               SkPorterDuff::Mode mode) {
  SkPaint paint;
  paint.setColor(color);
  paint.setStyle(SkPaint::kStroke_Style);
  // Contrary to the docs, a width of 0 results in nothing.
  paint.setStrokeWidth(SkIntToScalar(1));
  paint.setPorterDuffXfermode(mode);

  SkRect rc = {SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(x + w), SkIntToScalar(y + h) };
  drawRect(rc, paint);
}

void ChromeCanvas::DrawFocusRect(int x, int y, int width, int height) {
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

void ChromeCanvas::DrawBitmapInt(const SkBitmap& bitmap, int x, int y) {
  drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y));
}

void ChromeCanvas::DrawBitmapInt(const SkBitmap& bitmap, int x, int y,
                                 const SkPaint& paint) {
  drawBitmap(bitmap, SkIntToScalar(x), SkIntToScalar(y), &paint);
}

void ChromeCanvas::DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y,
                                 int src_w, int src_h, int dest_x, int dest_y,
                                 int dest_w, int dest_h,
                                 bool filter) {
  SkPaint p;
  DrawBitmapInt(bitmap, src_x, src_y, src_w, src_h, dest_x, dest_y,
                dest_w, dest_h, filter, p);
}

void ChromeCanvas::DrawBitmapInt(const SkBitmap& bitmap, int src_x, int src_y,
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
  shader_scale.setScale(
      SkFloatToScalar(static_cast<float>(dest_w) / src_w),
      SkFloatToScalar(static_cast<float>(dest_h) / src_h));
  shader_scale.postTranslate(SkIntToScalar(dest_x - src_x),
                             SkIntToScalar(dest_y - src_y));
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

// static
void ChromeCanvas::SizeStringInt(const std::wstring& text,
                                 const ChromeFont& font,
                                 int *width, int *height, int flags) {
  HDC dc = GetDC(NULL);
  HFONT old_font = static_cast<HFONT>(SelectObject(dc, font.hfont()));
  RECT b;
  b.left = 0;
  b.top = 0;
  b.right = *width;
  if (b.right == 0 && !text.empty()) {
    // Width needs to be at least 1 or else DoDrawText will not resize it.
    b.right = 1;
  }
  b.bottom = *height;
  DoDrawText(dc, text, &b, ComputeFormatFlags(flags) | DT_CALCRECT);

  // Restore the old font. This way we don't have to worry if the caller
  // deletes the font and the DC lives longer.
  SelectObject(dc, old_font);
  *width = b.right;
  *height = b.bottom;

  ReleaseDC(NULL, dc);
}

void ChromeCanvas::DrawStringInt(const std::wstring& text, HFONT font,
                                 const SkColor& color, int x, int y, int w,
                                 int h, int flags) {
  if (!IntersectsClipRectInt(x, y, w, h))
    return;

  getTopPlatformDevice().prepareForGDI(x, y, w, h);
  RECT text_bounds = { x, y, x + w, y + h };
  HDC dc = beginPlatformPaint();
  SetBkMode(dc, TRANSPARENT);
  HFONT old_font = (HFONT)SelectObject(dc, font);
  COLORREF brush_color = RGB(SkColorGetR(color), SkColorGetG(color),
                             SkColorGetB(color));
  SetTextColor(dc, brush_color);

  int f = ComputeFormatFlags(flags);
  DoDrawText(dc, text, &text_bounds, f);
  endPlatformPaint();

  // Restore the old font. This way we don't have to worry if the caller
  // deletes the font and the DC lives longer.
  SelectObject(dc, old_font);
  getTopPlatformDevice().postProcessGDI(x, y, w, h);
}

void ChromeCanvas::DrawStringInt(const std::wstring& text,
                                 const ChromeFont& font,
                                 const SkColor& color,
                                 int x, int y,
                                 int w, int h) {
  DrawStringInt(text, font, color, x, y, w, h,
                l10n_util::DefaultCanvasTextAlignment());
}

void ChromeCanvas::DrawStringInt(const std::wstring& text,
                                 const ChromeFont& font,
                                 const SkColor& color,
                                 int x, int y, int w, int h, int flags) {
  DrawStringInt(text, font.hfont(), color, x, y, w, h, flags);
}

void ChromeCanvas::TileImageInt(const SkBitmap& bitmap,
                                int x, int y, int w, int h,
                                SkPorterDuff::Mode mode) {
  if (!IntersectsClipRectInt(x, y, w, h))
    return;

  SkPaint paint;
  int bitmap_width = bitmap.width();
  int bitmap_height = bitmap.height();

  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  paint.setShader(shader);
  paint.setPorterDuffXfermode(mode);

  // CreateBitmapShader returns a Shader with a reference count of one, we
  // need to unref after paint takes ownership of the shader.
  shader->unref();
  save();
  translate(SkIntToScalar(x), SkIntToScalar(y));
  ClipRectInt(0, 0, w, h);
  drawPaint(paint);
  restore();
}

void ChromeCanvas::TileImageInt(const SkBitmap& bitmap,
                                int x, int y, int w, int h) {
  TileImageInt(bitmap, x, y, w, h, SkPorterDuff::kSrcOver_Mode);
}

SkBitmap ChromeCanvas::ExtractBitmap() {
  const SkBitmap& device_bitmap = getDevice()->accessBitmap(false);

  // Make a bitmap to return, and a canvas to draw into it. We don't just want
  // to call extractSubset or the copy constuctor, since we want an actual copy
  // of the bitmap.
  SkBitmap result;
  device_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);
  return result;
}
