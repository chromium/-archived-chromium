// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/widget/root_view.h"

#include "base/logging.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "skia/include/SkColor.h"

namespace views {

// TODO(port): Port GetScheduledPaintRectConstrainedToSize() to not use RECT.

void RootView::UpdateCursor(const MouseEvent& e) {
  NOTIMPLEMENTED();
}

// TODO(port): Port OnPaint() to not use HWNDs in its public interface.
void RootView::OnPaint(GdkEventExpose* event) {
  ChromeCanvasPaint canvas(event);
  canvas.FillRectInt(SK_ColorRED, 5, 5, 10, 10);
  canvas.FillRectInt(SK_ColorGREEN, 25, 5, 10, 10);
  canvas.FillRectInt(SK_ColorBLUE, 45, 5, 10, 10);

  if (!canvas.isEmpty()) {
    //    const PAINTSTRUCT& ps = canvas.paintStruct();
    //    SchedulePaint(gfx::Rect(ps.rcPaint), false);
    if (NeedsPainting(false))
      ProcessPaint(&canvas);
  }
}

}
