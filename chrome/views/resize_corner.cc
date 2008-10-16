// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/resize_corner.h"

#include <vssym32.h>

#include "base/gfx/native_theme.h"
#include "chrome/common/gfx/chrome_canvas.h"

namespace views {

ResizeCorner::ResizeCorner() {
}

ResizeCorner::~ResizeCorner() {
}

void ResizeCorner::Paint(ChromeCanvas* canvas) {
  // Paint the little handle.
  RECT widgetRect = { 0, 0, width(), height() };
  HDC hdc = canvas->beginPlatformPaint();
  gfx::NativeTheme::instance()->PaintStatusGripper(hdc,
                                                   SP_GRIPPER,
                                                   0,
                                                   0,
                                                   &widgetRect);
  canvas->endPlatformPaint();
}

}  // namespace views

