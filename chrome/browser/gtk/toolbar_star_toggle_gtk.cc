// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/toolbar_star_toggle_gtk.h"
#include "chrome/common/resource_bundle.h"
#include "grit/theme_resources.h"

ToolbarStarToggleGtk::ToolbarStarToggleGtk() {
  widget_ = gtk_button_new();

  // Load the button images from the resource bundle.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  unstarred_pixbuf_[GTK_STATE_NORMAL] = rb.LoadPixbuf(IDR_STAR);
  unstarred_pixbuf_[GTK_STATE_ACTIVE] = rb.LoadPixbuf(IDR_STAR_P);
  unstarred_pixbuf_[GTK_STATE_PRELIGHT] = rb.LoadPixbuf(IDR_STAR_H);
  unstarred_pixbuf_[GTK_STATE_SELECTED] = NULL;
  unstarred_pixbuf_[GTK_STATE_INSENSITIVE] = rb.LoadPixbuf(IDR_STAR_D);

  starred_pixbuf_[GTK_STATE_NORMAL] = rb.LoadPixbuf(IDR_STARRED);
  starred_pixbuf_[GTK_STATE_ACTIVE] = rb.LoadPixbuf(IDR_STARRED_P);
  starred_pixbuf_[GTK_STATE_PRELIGHT] = rb.LoadPixbuf(IDR_STARRED_H);
  starred_pixbuf_[GTK_STATE_SELECTED] = NULL;
  starred_pixbuf_[GTK_STATE_INSENSITIVE] = NULL;

  gtk_widget_set_size_request(widget_,
                              gdk_pixbuf_get_width(unstarred_pixbuf_[0]),
                              gdk_pixbuf_get_height(unstarred_pixbuf_[0]));

  gtk_widget_set_app_paintable(widget_, TRUE);
  // We effectively double-buffer by virtue of having only one image...
  gtk_widget_set_double_buffered(widget_, FALSE);
  g_signal_connect(G_OBJECT(widget_), "expose-event",
                   G_CALLBACK(OnExpose), this);
}

ToolbarStarToggleGtk::~ToolbarStarToggleGtk() {
  for (size_t i = 0; i < arraysize(unstarred_pixbuf_); ++i) {
    if (unstarred_pixbuf_[i])
      gdk_pixbuf_unref(unstarred_pixbuf_[i]);
    if (starred_pixbuf_[i])
      gdk_pixbuf_unref(starred_pixbuf_[i]);
  }
}

void ToolbarStarToggleGtk::SetStarred(bool starred) {
  starred_ = starred;
  gtk_widget_queue_draw(widget_);
}

// static
gboolean ToolbarStarToggleGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                        ToolbarStarToggleGtk* button) {
  GdkPixbuf** pixbuf_bank = NULL;
  if (button->starred_)
    pixbuf_bank = button->starred_pixbuf_;
  else
    pixbuf_bank = button->unstarred_pixbuf_;

  GdkPixbuf* pixbuf = pixbuf_bank[GTK_WIDGET_STATE(widget)];

  // Fall back to the default image if we don't have one for this state.
  if (!pixbuf)
    pixbuf = pixbuf_bank[GTK_STATE_NORMAL];

  if (!pixbuf)
    return FALSE;

  gdk_draw_pixbuf(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  pixbuf,
                  0, 0,
                  widget->allocation.x, widget->allocation.y, -1, -1,
                  GDK_RGB_DITHER_NONE, 0, 0);

  return TRUE;
}
