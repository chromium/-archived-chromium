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
  gfx::Rect original_dirty_region = GetScheduledPaintRectConstrainedToSize();
  if (!original_dirty_region.IsEmpty()) {
    // Between the the time the paint was scheduled and the time we end
    // up painting more SchedulePaints may have been invoked. Expand the
    // region Gdk wants us to paint to include the region we want to paint
    // to make sure everything is painted. Otherwise we may not paint
    // everything we need to.
    gfx::Rect complete_area =
        original_dirty_region.Union(gfx::Rect(event->area));
    event->area = complete_area.ToGdkRectangle();
  }

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
