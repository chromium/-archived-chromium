// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/link_button_gtk.h"

static const char* kLinkMarkup = "<u><span color=\"%s\">%s</span></u>";

namespace {

// Set the GTK style on our custom link button. We don't want any border around
// the link text.
void SetLinkButtonStyle() {
  static bool style_was_set = false;

  if (style_was_set)
    return;
  style_was_set = true;

  gtk_rc_parse_string(
      "style \"chrome-link-button\" {"
      "  GtkButton::inner-border = {0, 0, 0, 0}"
      "  xthickness = 0"
      "  ythickness = 0"
      "}"
      "widget \"*chrome-link-button\" style \"chrome-link-button\"");
}

}  // namespace

LinkButtonGtk::LinkButtonGtk(const char* text)
    : hand_cursor_(gdk_cursor_new(GDK_HAND2)),
      is_blue_(true) {
  SetLinkButtonStyle();

  // We put a label in a button so we can connect to the click event. We don't
  // let the button draw itself; catch all expose events to the button and pass
  // them through to the label.
  label_ = gtk_label_new(NULL);
  blue_markup = g_markup_printf_escaped(kLinkMarkup, "blue", text);
  red_markup = g_markup_printf_escaped(kLinkMarkup, "red", text);
  gtk_label_set_markup(GTK_LABEL(label_), blue_markup);

  widget_.Own(gtk_button_new());
  gtk_container_add(GTK_CONTAINER(widget_.get()), label_);
  gtk_widget_set_name(widget_.get(), "chrome-link-button");
  gtk_widget_set_app_paintable(widget_.get(), TRUE);
  g_signal_connect(widget_.get(), "expose-event",
                   G_CALLBACK(OnExpose), this);
  // We connect to these signals so we can set the cursor appropriately. We
  // could give the link button its own GdkWindow (e.g. by placing it in a
  // GtkEventBox), but that would wreak havok with painting of the parent
  // widget. We can't use the enter- and leave- notify events as they operate
  // on the widget's GdkWindow, and |label| nor |button| has its own GdkWindow.
  g_signal_connect(widget_.get(), "enter",
                   G_CALLBACK(OnEnter), this);
  g_signal_connect(widget_.get(), "leave",
                   G_CALLBACK(OnLeave), this);
}

LinkButtonGtk::~LinkButtonGtk() {
  g_free(red_markup);
  g_free(blue_markup);
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
                                 LinkButtonGtk* link_button) {
  GtkWidget* label = link_button->label_;

  if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE && link_button->is_blue_) {
    gtk_label_set_markup(GTK_LABEL(label), link_button->red_markup);
    link_button->is_blue_ = false;
  } else if (GTK_WIDGET_STATE(widget) != GTK_STATE_ACTIVE &&
             !link_button->is_blue_) {
    gtk_label_set_markup(GTK_LABEL(label), link_button->blue_markup);
    link_button->is_blue_ = true;
  }

  // Draw the link inside the button.
  gtk_container_propagate_expose(GTK_CONTAINER(widget), label, event);

  // Draw the focus rectangle.
  if (GTK_WIDGET_HAS_FOCUS(widget)) {
    gtk_paint_focus(widget->style, widget->window,
                    static_cast<GtkStateType>(GTK_WIDGET_STATE(widget)),
                    &event->area, widget, NULL,
                    widget->allocation.x, widget->allocation.y,
                    widget->allocation.width, widget->allocation.height);
  }

  // Don't let the button draw itself, ever.
  return TRUE;
}
