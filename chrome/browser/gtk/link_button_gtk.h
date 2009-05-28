// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "chrome/common/owned_widget_gtk.h"

// Creates a link button that shows |text| in blue and underlined. The cursor
// changes to a hand when over the link.
class LinkButtonGtk {
 public:
  explicit LinkButtonGtk(const char* text);
  virtual ~LinkButtonGtk();

  GtkWidget* widget() { return widget_.get(); }

  GtkWidget* label() { return label_; }

 private:
  // Called when the pointer enters or leaves the button.
  static gboolean OnEnter(GtkWidget* widget, LinkButtonGtk* link_button);
  static gboolean OnLeave(GtkWidget* widget, LinkButtonGtk* link_button);

  // Called when the pointer moves over the link button's gdk window.
  static gboolean OnMotionNotify(GtkWidget* widget,
                                 GdkEventMotion* event,
                                 LinkButtonGtk* link_button);

  // Called when the widget is exposed.
  static gboolean OnExpose(GtkWidget* widget,
                           GdkEventExpose* event,
                           LinkButtonGtk* link_button);

  // The button widget.
  OwnedWidgetGtk widget_;

  // The text widget.
  GtkWidget* label_;

  // We keep this hand cursor so we don't have to recreate it every time the
  // user mouses over the link.
  GdkCursor* hand_cursor_;

  // Text markup for the link. We use the red one when the link is being
  // clicked.
  gchar* blue_markup;
  gchar* red_markup;
  // The current state of the text.
  bool is_blue_;
};
