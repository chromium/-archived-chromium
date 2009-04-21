// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include <gtk/gtk.h>

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

void ResourceMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                            gfx::Rect* rect) {
  if (!window_id) {
    *rect = gfx::Rect();
    return;
  }

  // Ideally this would be gtk_widget_get_window but that's only
  // from gtk 2.14 onwards. :(
  GdkWindow* window = gfx::NativeViewFromId(window_id)->window;
  DCHECK(window);
  gint x, y, width, height;
  // This is the "position of a window in root window coordinates".
  gdk_window_get_origin(window, &x, &y);
  gdk_window_get_size(window, &width, &height);
  *rect = gfx::Rect(x, y, width, height);
}

void ResourceMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                                gfx::Rect* rect) {
  if (!window_id) {
    *rect = gfx::Rect();
    return;
  }

  // Windows uses GetAncestor(window, GA_ROOT) here which probably means
  // we want the top level window.
  GdkWindow* window =
  gdk_window_get_toplevel(gfx::NativeViewFromId(window_id)->window);
  DCHECK(window);
  gint x, y, width, height;
  // This is the "position of a window in root window coordinates".
  gdk_window_get_origin(window, &x, &y);
  gdk_window_get_size(window, &width, &height);
  *rect = gfx::Rect(x, y, width, height);
}
