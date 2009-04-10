// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/link_button_gtk.h"

static const char* kLinkMarkup = "<u><span color=\"blue\">%s</span></u>";

LinkButtonGtk::LinkButtonGtk(const char* text)
    : hand_cursor_(gdk_cursor_new(GDK_HAND2)) {
  // We put a label in a button so we can connect to the click event. We don't
  // let the button draw itself; catch all expose events to the button and pass
  // them through to the label.
  // TODO(estade): the link should turn red during the user's click.
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kLinkMarkup, text);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  widget_.Own(gtk_button_new());
  gtk_widget_set_app_paintable(widget_.get(), TRUE);
  g_signal_connect(widget_.get(), "expose-event",
                   G_CALLBACK(OnExpose), NULL);
  // We connect to these signals so we can set the cursor appropriately. We
  // could give the link button its own GdkWindow (e.g. by placing it in a
  // GtkEventBox), but that would wreak havok with painting of the parent
  // widget. We can't use the enter- and leave- notify events as they operate
  // on the widget's GdkWindow, and |label| nor |button| has its own GdkWindow.
  g_signal_connect(widget_.get(), "enter",
                   G_CALLBACK(OnEnter), this);
  g_signal_connect(widget_.get(), "leave",
                   G_CALLBACK(OnLeave), this);
  gtk_container_add(GTK_CONTAINER(widget_.get()), label);
}

LinkButtonGtk::~LinkButtonGtk() {
  gdk_cursor_unref(hand_cursor_);
  widget_.Destroy();
}

// static
gboolean LinkButtonGtk::OnEnter(GtkWidget* widget,
                                LinkButtonGtk* link_button) {
  gdk_window_set_cursor(widget->window, link_button->hand_cursor_);
  return FALSE;
}

// static
gboolean LinkButtonGtk::OnLeave(GtkWidget* widget,
                                LinkButtonGtk* link_button) {
  gdk_window_set_cursor(widget->window, NULL);
  return FALSE;
}

// static
gboolean LinkButtonGtk::OnExpose(GtkWidget* widget,
                                 GdkEventExpose* event,
                                 gpointer user_data) {
  // Draw the link inside the button.
  gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                 gtk_bin_get_child(GTK_BIN(widget)),
                                 event);
  // Don't let the button draw itself, ever.
  return TRUE;
}
