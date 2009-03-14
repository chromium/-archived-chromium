// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/native_frame_view.h"

#include "chrome/views/window.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, public:

NativeFrameView::NativeFrameView(Window* frame)
    : NonClientFrameView(),
      frame_(frame) {
}

NativeFrameView::~NativeFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, NonClientFrameView overrides:

gfx::Rect NativeFrameView::GetBoundsForClientView() const {
  return gfx::Rect(0, 0, width(), height());
}

gfx::Rect NativeFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  RECT rect = client_bounds.ToRECT();
  AdjustWindowRectEx(&rect, frame_->window_style(), FALSE,
                     frame_->window_ex_style());
  return gfx::Rect(rect);
}

gfx::Point NativeFrameView::GetSystemMenuPoint() const {
  POINT temp = {0, -kFrameShadowThickness };
  MapWindowPoints(frame_->GetNativeView(), HWND_DESKTOP, &temp, 1);
  return gfx::Point(temp);
}

int NativeFrameView::NonClientHitTest(const gfx::Point& point) {
  return HTNOWHERE;
}

void NativeFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
  // Nothing to do, we use the default window mask.
}

void NativeFrameView::EnableClose(bool enable) {
  // Nothing to do, handled automatically by Window.
}

void NativeFrameView::ResetWindowControls() {
  // Nothing to do.
}

}  // namespace views
