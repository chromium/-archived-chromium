// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "app/gfx/font.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {

// Returns a new pango font, free with pango_font_description_free().
PangoFontDescription* PangoFontFromGfxFont(const gfx::Font& gfx_font) {
  gfx::Font font = gfx_font;  // Copy so we can call non-const methods.
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font.FontName()).c_str());
  pango_font_description_set_size(pfd, font.FontSize() * PANGO_SCALE);

  switch (font.style()) {
    case gfx::Font::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case gfx::Font::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case gfx::Font::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case gfx::Font::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

}  // namespace

namespace gfx {

Canvas::Canvas(int width, int height, bool is_opaque)
    : skia::PlatformCanvas(width, height, is_opaque) {
}

Canvas::Canvas() : skia::PlatformCanvas() {
}

Canvas::~Canvas() {
}

// static
void Canvas::SizeStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           int* width, int* height, int flags) {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
  cairo_t* cr = cairo_create(surface);
  PangoLayout* layout = pango_cairo_create_layout(cr);

  if (flags & NO_ELLIPSIS) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
  } else {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  }

  if (flags & TEXT_ALIGN_CENTER) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
  } else if (flags & TEXT_ALIGN_RIGHT) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
  }

  if (flags & MULTI_LINE) {
    pango_layout_set_wrap(layout,
        (flags & CHARACTER_BREAK) ? PANGO_WRAP_WORD_CHAR : PANGO_WRAP_WORD);
  }

  std::string utf8 = WideToUTF8(text);
  pango_layout_set_text(layout, utf8.data(), utf8.size());
  PangoFontDescription* desc = PangoFontFromGfxFont(font);
  pango_layout_set_font_description(layout, desc);

  pango_layout_get_size(layout, width, height);
  *width /= PANGO_SCALE;
  *height /= PANGO_SCALE;
  g_object_unref(layout);
  pango_font_description_free(desc);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

void Canvas::ApplySkiaMatrixToCairoContext(cairo_t* cr) {
  const SkMatrix& skia_matrix = getTotalMatrix();
  cairo_matrix_t cairo_matrix;
  cairo_matrix_init(&cairo_matrix,
                    SkScalarToFloat(skia_matrix.getScaleX()),
                    SkScalarToFloat(skia_matrix.getSkewY()),
                    SkScalarToFloat(skia_matrix.getSkewX()),
                    SkScalarToFloat(skia_matrix.getScaleY()),
                    SkScalarToFloat(skia_matrix.getTranslateX()),
                    SkScalarToFloat(skia_matrix.getTranslateY()));
  cairo_set_matrix(cr, &cairo_matrix);
}

void Canvas::DrawStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           const SkColor& color, int x, int y, int w, int h,
                           int flags) {
  cairo_surface_t* surface = beginPlatformPaint();
  cairo_t* cr = cairo_create(surface);
  // We're going to draw onto the surface directly. This circumvents the matrix
  // installed by Skia. Apply the matrix from skia to cairo so they align and
  // we draw at the right place.
  ApplySkiaMatrixToCairoContext(cr);
  PangoLayout* layout = pango_cairo_create_layout(cr);

  cairo_set_source_rgb(cr,
                       SkColorGetR(color) / 255.0,
                       SkColorGetG(color) / 255.0,
                       SkColorGetB(color) / 255.0);

  // TODO(deanm): Implement the rest of the Canvas flags.
  if (!(flags & Canvas::NO_ELLIPSIS))
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

  pango_layout_set_width(layout, w * PANGO_SCALE);
  pango_layout_set_height(layout, h * PANGO_SCALE);

  if (flags & TEXT_ALIGN_CENTER) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
  } else if (flags & TEXT_ALIGN_RIGHT) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
  }

  if (flags & MULTI_LINE) {
    pango_layout_set_wrap(layout,
        (flags & CHARACTER_BREAK) ? PANGO_WRAP_WORD_CHAR : PANGO_WRAP_WORD);
  }

  if (flags & NO_ELLIPSIS)
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);

  std::string utf8 = WideToUTF8(text);
  pango_layout_set_text(layout, utf8.data(), utf8.size());

  PangoFontDescription* desc = PangoFontFromGfxFont(font);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);

  int width, height;
  pango_layout_get_size(layout, &width, &height);

  if (flags & Canvas::TEXT_VALIGN_TOP) {
    // Cairo should draw from the top left corner already.
  } else if (flags & Canvas::TEXT_VALIGN_BOTTOM) {
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

}  // namespace gfx
