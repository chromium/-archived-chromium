// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_canvas.h"

#include <pango/pango.h>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/gfx/chrome_font.h"

namespace {

// Returns a new pango font, free with pango_font_description_free().
PangoFontDescription* PangoFontFromChromeFont(const ChromeFont& chrome_font) {
  ChromeFont font = chrome_font;  // Copy so we can call non-const methods.
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font.FontName()).c_str());
  pango_font_description_set_size(pfd, font.FontSize() * PANGO_SCALE);

  switch (font.style()) {
    case ChromeFont::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case ChromeFont::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case ChromeFont::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case ChromeFont::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

}  // namespace

ChromeCanvas::ChromeCanvas(int width, int height, bool is_opaque)
    : skia::PlatformCanvasLinux(width, height, is_opaque) {
}

ChromeCanvas::ChromeCanvas() : skia::PlatformCanvasLinux() {
}

ChromeCanvas::~ChromeCanvas() {
}

// static
void ChromeCanvas::SizeStringInt(const std::wstring& text,
                                 const ChromeFont& font,
                                 int* width, int* height, int flags) {
  NOTIMPLEMENTED();
}


void ChromeCanvas::DrawStringInt(const std::wstring& text,
                                 const ChromeFont& font,
                                 const SkColor& color, int x, int y, int w,
                                 int h, int flags) {
  cairo_surface_t* surface = beginPlatformPaint();
  cairo_t* cr = cairo_create(surface);
  PangoLayout* layout = pango_cairo_create_layout(cr);

  cairo_set_source_rgb(cr,
                       SkColorGetR(color) / 255.0,
                       SkColorGetG(color) / 255.0,
                       SkColorGetB(color) / 255.0);

  // TODO(deanm): Implement the rest of the ChromeCanvas flags.
  if (!(flags & ChromeCanvas::NO_ELLIPSIS))
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

  pango_layout_set_width(layout, w * PANGO_SCALE);
  pango_layout_set_height(layout, h * PANGO_SCALE);

  std::string utf8 = WideToUTF8(text);
  pango_layout_set_text(layout, utf8.data(), utf8.size());

  PangoFontDescription* desc = PangoFontFromChromeFont(font);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);

  int width, height;
  pango_layout_get_size(layout, &width, &height);

  if (flags & ChromeCanvas::TEXT_VALIGN_TOP) {
    // Cairo should draw from the top left corner already.
  } else if (flags & ChromeCanvas::TEXT_VALIGN_BOTTOM) {
    y = y + (h - (height / PANGO_SCALE));
  } else {
    // Vertically centered.
    y = y + ((h - (height / PANGO_SCALE)) / 2);
  }

  cairo_move_to(cr, x, y);
  pango_cairo_show_layout(cr, layout);

  g_object_unref(layout);
  cairo_destroy(cr);
  // NOTE: beginPlatformPaint returned its surface, we shouldn't destroy it.
}
