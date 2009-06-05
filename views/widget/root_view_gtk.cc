// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include <gtk/gtk.h>

#include "app/gfx/canvas.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"

namespace views {

void RootView::OnPaint(GdkEventExpose* event) {
  gfx::CanvasPaint canvas(event);

  if (!canvas.isEmpty()) {
    SchedulePaint(gfx::Rect(canvas.rectangle()), false);
    if (NeedsPainting(false)) {
      ProcessPaint(&canvas);
    }
  }
}

}
