// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_canvas.h"

#include <limits>

#include "base/gfx/rect.h"
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
  // Only adjust string directionality if both of the following are true:
  // 1. The current locale is RTL.
  // 2. The string itself has RTL directionality.
  if (flags & DT_RTLREADING) {
    if (l10n_util::AdjustStringForLocaleDirection(text, &localized_text)) {
      string_ptr = localized_text.c_str();
      string_size = static_cast<int>(localized_text.length());
    }
  }

  DrawText(hdc, string_ptr, string_size, text_bounds, flags);
}

// Compute the windows flags necessary to implement the provided text
// ChromeCanvas flags.
int ComputeFormatFlags(int flags, const std::wstring& text) {
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
  //
  // We make a distinction between Chrome UI strings and text coming from a web
  // page.
  //
  // For text coming from a web page we determine the alignment based on the
  // first character with strong directionality. If the directionality of the
  // first character with strong directionality in the text is LTR, the
  // alignment is set to DT_LEFT, and the directionality should not be set as
  // DT_RTLREADING.
  //
  // This heuristic doesn't work for Chrome UI strings since even in RTL
  // locales, some of those might start with English text but we know they're
  // localized so we always want them to be right aligned, and their
  // directionality should be set as DT_RTLREADING.
  //
  // Caveat: If the string is purely LTR, don't set DTL_RTLREADING since when
  // the flag is set, LRE-PDF don't have the desired effect of rendering
  // multiline English-only text as LTR.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT &&
      (f & DT_RIGHT)) {
    if (l10n_util::StringContainsStrongRTLChars(text)) {
      f |= DT_RTLREADING;
    }
  }
  return f;
}

}  // anonymous namespace

ChromeCanvas::ChromeCanvas(int width, int height, bool is_opaque)
    : skia::PlatformCanvasWin(width, height, is_opaque) {
}

ChromeCanvas::ChromeCanvas() : skia::PlatformCanvasWin() {
}

ChromeCanvas::~ChromeCanvas() {
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
  DoDrawText(dc, text, &b, ComputeFormatFlags(flags, text) | DT_CALCRECT);

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

  RECT text_bounds = { x, y, x + w, y + h };
  HDC dc = beginPlatformPaint();
  SetBkMode(dc, TRANSPARENT);
  HFONT old_font = (HFONT)SelectObject(dc, font);
  COLORREF brush_color = RGB(SkColorGetR(color), SkColorGetG(color),
                             SkColorGetB(color));
  SetTextColor(dc, brush_color);

  int f = ComputeFormatFlags(flags, text);
  DoDrawText(dc, text, &text_bounds, f);
  endPlatformPaint();

  // Restore the old font. This way we don't have to worry if the caller
  // deletes the font and the DC lives longer.
  SelectObject(dc, old_font);

  // Windows will have cleared the alpha channel of the text we drew. Assume
  // we're drawing to an opaque surface, or at least the text rect area is
  // opaque.
  getTopPlatformDevice().makeOpaque(x, y, w, h);
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

// Checks each pixel immediately adjacent to the given pixel in the bitmap. If
// any of them are not the halo color, returns true. This defines the halo of
// pixels that will appear around the text. Note that we have to check each
// pixel against both the halo color and transparent since DrawStringWithHalo
// will modify the bitmap as it goes, and clears pixels shouldn't count as
// changed.
static bool pixelShouldGetHalo(const SkBitmap& bitmap, int x, int y,
                               SkColor halo_color) {
  if (x > 0 &&
      *bitmap.getAddr32(x - 1, y) != halo_color &&
      *bitmap.getAddr32(x - 1, y) != 0)
    return true;  // Touched pixel to the left.
  if (x < bitmap.width() - 1 &&
      *bitmap.getAddr32(x + 1, y) != halo_color &&
      *bitmap.getAddr32(x + 1, y) != 0)
    return true;  // Touched pixel to the right.
  if (y > 0 &&
      *bitmap.getAddr32(x, y - 1) != halo_color &&
      *bitmap.getAddr32(x, y - 1) != 0)
    return true;  // Touched pixel above.
  if (y < bitmap.height() - 1 &&
      *bitmap.getAddr32(x, y + 1) != halo_color &&
      *bitmap.getAddr32(x, y + 1) != 0)
    return true;  // Touched pixel below.
  return false;
}

void ChromeCanvas::DrawStringWithHalo(const std::wstring& text,
                                      const ChromeFont& font,
                                      const SkColor& text_color,
                                      const SkColor& halo_color_in,
                                      int x, int y, int w, int h,
                                      int flags) {
  // Some callers will have semitransparent halo colors, which we don't handle
  // (since the resulting image can have 1-bit transparency only).
  SkColor halo_color = halo_color_in | 0xFF000000;

  // Create a temporary buffer filled with the halo color. It must leave room
  // for the 1-pixel border around the text.
  ChromeCanvas text_canvas(w + 2, h + 2, true);
  SkPaint bkgnd_paint;
  bkgnd_paint.setColor(halo_color);
  text_canvas.FillRectInt(0, 0, w + 2, h + 2, bkgnd_paint);

  // Draw the text into the temporary buffer. This will have correct
  // ClearType since the background color is the same as the halo color.
  text_canvas.DrawStringInt(text, font, text_color, 1, 1, w, h, flags);

  // Windows will have cleared the alpha channel for the pixels it drew. Make it
  // opaque. We have to do this first since pixelShouldGetHalo will check for
  // 0 to see if a pixel has been modified to transparent, and black text that
  // Windows draw will look transparent to it!
  text_canvas.getTopPlatformDevice().makeOpaque(0, 0, w + 2, h + 2);

  uint32_t halo_premul = SkPreMultiplyColor(halo_color);
  SkBitmap& text_bitmap = const_cast<SkBitmap&>(
      text_canvas.getTopPlatformDevice().accessBitmap(true));
  for (int cur_y = 0; cur_y < h + 2; cur_y++) {
    uint32_t* text_row = text_bitmap.getAddr32(0, cur_y);
    for (int cur_x = 0; cur_x < w + 2; cur_x++) {
      if (text_row[cur_x] == halo_premul) {
        // This pixel was not touched by the text routines. See if it borders
        // a touched pixel in any of the 4 directions (not diagonally).
        if (!pixelShouldGetHalo(text_bitmap, cur_x, cur_y, halo_premul))
          text_row[cur_x] = 0;  // Make transparent.
      } else {
        text_row[cur_x] |= 0xff << SK_A32_SHIFT;  // Make opaque.
      }
    }
  }

  // Draw the halo bitmap with blur.
  drawBitmap(text_bitmap, SkIntToScalar(x - 1), SkIntToScalar(y - 1));
}
