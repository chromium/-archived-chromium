// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_canvas.h"

#include <limits>

#include "base/gfx/rect.h"
#include "base/sys_string_conversions.h"
#include "skia/include/SkShader.h"
#include "skia/include/SkPaint.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"

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
  // TODO(port): @flags is currently ignored
  SkPaint paint;
  font.PaintSetup(&paint);

  const std::string utf8(base::SysWideToUTF8(text));
  paint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
  SkRect bounds;
  paint.measureText(utf8.data(), utf8.size(), &bounds);

  *width = bounds.width();
  *height = bounds.height();
}

void ChromeCanvas::DrawStringInt(const std::wstring& text,
                                 const ChromeFont& font,
                                 const SkColor& color, int x, int y, int w,
                                 int h, int flags) {
  // TODO(port): @flags, @w and @h are currently ignored
  SkPaint paint;
  font.PaintSetup(&paint);

  const std::string utf8(base::SysWideToUTF8(text));
  paint.setTextEncoding(SkPaint::kUTF8_TextEncoding);
  paint.setColor(color);

  drawText(utf8.data(), utf8.size(), x, y, paint);
}

void ChromeCanvas::DrawStringInt(const std::wstring& text,
                                 const ChromeFont& font,
                                 const SkColor& color,
                                 int x, int y,
                                 int w, int h) {
  DrawStringInt(text, font, color, x, y, w, h,
                l10n_util::DefaultCanvasTextAlignment());
}
