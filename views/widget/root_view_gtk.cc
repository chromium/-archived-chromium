// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <gtk/gtk.h>

#include "app/gfx/canvas_paint.h"
#include "base/logging.h"
#include "views/widget/widget_gtk.h"

namespace views {

void RootView::OnPaint(GdkEventExpose* event) {
  gfx::CanvasPaint canvas(event);

  if (!canvas.is_empty()) {
    canvas.set_composite_alpha(
        static_cast<WidgetGtk*>(GetWidget())->is_transparent());
    SchedulePaint(gfx::Rect(canvas.rectangle()), false);
    if (NeedsPainting(false)) {
      ProcessPaint(&canvas);
    }
  }
}

}
