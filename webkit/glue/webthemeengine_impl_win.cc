// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webthemeengine_impl_win.h"

#include "WebRect.h"

#include "base/gfx/native_theme.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;

namespace webkit_glue {

static RECT WebRectToRECT(const WebRect& rect) {
  RECT result;
  result.left = rect.x;
  result.top = rect.y;
  result.right = rect.x + rect.width;
  result.bottom = rect.y + rect.height;
  return result;
}

void WebThemeEngineImpl::paintButton(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintButton(
      hdc, part, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void WebThemeEngineImpl::paintMenuList(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintMenuList(
      hdc, part, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void WebThemeEngineImpl::paintScrollbarArrow(
    WebCanvas* canvas, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintScrollbarArrow(
      hdc, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void WebThemeEngineImpl::paintScrollbarThumb(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeTheme::instance()->PaintScrollbarThumb(
      hdc, part, state, classic_state, &native_rect);

  canvas->endPlatformPaint();
}

void WebThemeEngineImpl::paintScrollbarTrack(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, const WebRect& align_rect) {
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = WebRectToRECT(rect);
  RECT native_align_rect = WebRectToRECT(align_rect);
  gfx::NativeTheme::instance()->PaintScrollbarTrack(
      hdc, part, state, classic_state, &native_rect, &native_align_rect,
      canvas);

  canvas->endPlatformPaint();
}

void WebThemeEngineImpl::paintTextField(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, WebColor color, bool fill_content_area,
    bool draw_edges) {
  HDC hdc = canvas->beginPlatformPaint();

  RECT native_rect = WebRectToRECT(rect);

  gfx::NativeTheme::instance()->PaintTextField(
      hdc, part, state, classic_state, &native_rect, color, fill_content_area,
      draw_edges);

  canvas->endPlatformPaint();
}

}  // namespace webkit_glue
