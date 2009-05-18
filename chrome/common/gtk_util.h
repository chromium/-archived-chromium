// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_UTIL_H_
#define CHROME_COMMON_GTK_UTIL_H_

#include <gtk/gtk.h>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "webkit/glue/window_open_disposition.h"

typedef struct _GtkWidget GtkWidget;

namespace event_utils {

// Translates event flags into what kind of disposition they represent.
// For example, a middle click would mean to open a background tab.
// event_flags are the state in the GdkEvent structure.
WindowOpenDisposition DispositionFromEventFlags(guint state);

}  // namespace event_utils

namespace gtk_util {

  // Create a GtkBin with |child| as its child widget.  This bin will paint a
// border of color |color| with the sizes specified in pixels.
GtkWidget* CreateGtkBorderBin(GtkWidget* child, const GdkColor* color,
                              int top, int bottom, int left, int right);

// Remove all children from this container.
void RemoveAllChildren(GtkWidget* container);

// Force the font size of the widget to |size_pixels|.
void ForceFontSizePixels(GtkWidget* widget, double size_pixels);

// Gets the position of a gtk widget in screen coordinates.
gfx::Point GetWidgetScreenPosition(GtkWidget* widget);

// Returns the bounds of the specified widget in screen coordinates.
gfx::Rect GetWidgetScreenBounds(GtkWidget* widget);

// Initialize some GTK settings so that our dialogs are consistent.
void InitRCStyles();

}  // namespace gtk_util

#endif  // CHROME_COMMON_GTK_UTIL_H_
