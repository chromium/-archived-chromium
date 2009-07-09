// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_UTIL_H_
#define CHROME_COMMON_GTK_UTIL_H_

#include <gtk/gtk.h>
#include <string>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "chrome/common/x11_util.h"
#include "webkit/glue/window_open_disposition.h"

typedef struct _GtkWidget GtkWidget;

namespace event_utils {

// Translates event flags into what kind of disposition they represent.
// For example, a middle click would mean to open a background tab.
// event_flags are the state in the GdkEvent structure.
WindowOpenDisposition DispositionFromEventFlags(guint state);

// Get the timestamp (milliseconds) out of a GdkEvent.
// Returns 0 if the event has no timestamp.
// TODO(evanm): see comments in jankometer.cc about using this.
guint32 GetGdkEventTime(GdkEvent* event);

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

// Create a table of labeled controls, using proper spacing and alignment.
// Arguments should be pairs of const char*, GtkWidget*, concluding with a NULL.
// For example:
// controls = CreateLabeledControlsGroup("Name:", title_entry_,
//                                       "Folder:", folder_combobox_,
//                                       NULL);
GtkWidget* CreateLabeledControlsGroup(const char* text, ...);

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

// Converts a point in a widget to screen coordinates.  The point |p| is
// relative to the widget's top-left origin.
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

// Enumerates the top-level gdk windows of the current display.
void EnumerateTopLevelWindows(x11_util::EnumerateWindowsDelegate* delegate);

// Set that a button causes a page navigation. In particular, it will accept
// middle clicks. Warning: only call this *after* you have connected your
// own handlers for button-press and button-release events, or you will not get
// those events.
void SetButtonTriggersNavigation(GtkWidget* button);

// Returns the mirrored x value for |bounds| if the layout is RTL; otherwise,
// the original value is returned unchanged.
int MirroredLeftPointForRect(GtkWidget* widget, const gfx::Rect& bounds);

// Returns true if the pointer is currently inside the widget.
bool WidgetContainsCursor(GtkWidget* widget);

}  // namespace gtk_util

#endif  // CHROME_COMMON_GTK_UTIL_H_
