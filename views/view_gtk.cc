// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include <gtk/gtk.h>

#include "base/logging.h"

namespace views {

void View::DoDrag(const MouseEvent& e, int press_x, int press_y) {
  NOTIMPLEMENTED();
}

ViewAccessibilityWrapper* View::GetViewAccessibilityWrapper() {
  NOTIMPLEMENTED();
  return NULL;
}

void View::Focus() {
  NOTIMPLEMENTED();
}

int View::GetHorizontalDragThreshold() {
  static bool determined_threshold = false;
  static int drag_threshold = 8;
  if (determined_threshold)
    return drag_threshold;
  determined_threshold = true;
  GtkSettings* settings = gtk_settings_get_default();
  if (!settings)
    return drag_threshold;
  int value = 0;
  g_object_get(G_OBJECT(settings), "gtk-dnd-drag-threshold", &value, NULL);
  if (value)
    drag_threshold = value;
  return drag_threshold;
}

int View::GetVerticalDragThreshold() {
  // Vertical and horizontal drag threshold are the same in Gtk.
  return GetHorizontalDragThreshold();
}

}  // namespace views
