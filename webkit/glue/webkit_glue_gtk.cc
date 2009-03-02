// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include <gtk/gtk.h>

#include "webkit/glue/screen_info.h"

namespace webkit_glue {

ScreenInfo GetScreenInfoHelper(gfx::NativeView window) {
  GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(window));
  GdkVisual* visual = gdk_screen_get_system_visual(screen);

  ScreenInfo results;
  results.rect.SetRect(0, 0, gdk_screen_get_width(screen),
                       gdk_screen_get_height(screen));
  // I don't know of a way to query the "maximise" size of the window (e.g.
  // screen size less sidebars etc) since this is something which only the
  // window manager knows.
  results.available_rect = results.rect;

  results.depth = visual->depth;
  results.depth_per_component = visual->bits_per_rgb;

  results.is_monochrome = false;

  return results;
}

}  // namespace webkit_glue
