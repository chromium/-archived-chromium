// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include <gtk/gtk.h>

namespace webkit_glue {

// TODO: complete this function
ScreenInfo GetScreenInfoHelper(gfx::ViewHandle window) {
  GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(window));

  ScreenInfo results;
  results.rect.SetRect(0, 0, gdk_screen_get_width(screen),
                       gdk_screen_get_height(screen));

  return results;
}

}  // namespace webkit_glue

