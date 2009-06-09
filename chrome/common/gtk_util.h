// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_UTIL_H_
#define CHROME_COMMON_GTK_UTIL_H_

#include <gtk/gtk.h>
#include <string>

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

// Constants relating to the layout of dialog windows:
// (See http://library.gnome.org/devel/hig-book/stable/design-window.html.en)

// Spacing between controls of the same group.
const int kControlSpacing = 6;

// Horizontal spacing between a label and its control.
const int kLabelSpacing = 12;

// Indent of the controls within each group.
const int kGroupIndent = 12;

// Space around the outsides of a dialog's contents.
const int kContentAreaBorder = 12;

// Spacing between groups of controls.
const int kContentAreaSpacing = 18;

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

// Converts a point in a widget to screen coordinates.
void ConvertWidgetPointToScreen(GtkWidget* widget, gfx::Point* p);

// Initialize some GTK settings so that our dialogs are consistent.
void InitRCStyles();

// Stick the widget in the given hbox without expanding vertically. The widget
// is packed at the start of the hbox. This is useful for widgets that would
// otherwise expand to fill the vertical space of the hbox (e.g. buttons).
void CenterWidgetInHBox(GtkWidget* hbox, GtkWidget* widget, bool pack_at_end,
                        int padding);

// Change windows accelerator style to GTK style. (GTK uses _ for
// accelerators.  Windows uses & with && as an escape for &.)
std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label);

// Returns true if the screen is composited, false otherwise.
bool IsScreenComposited();

}  // namespace gtk_util

#endif  // CHROME_COMMON_GTK_UTIL_H_
