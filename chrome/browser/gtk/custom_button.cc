// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/custom_button.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/browser/gtk/nine_box.h"

#include "grit/theme_resources.h"

CustomDrawButton::CustomDrawButton(int normal_id,
    int active_id, int highlight_id, int depressed_id) {
  widget_ = gtk_button_new();

  // Load the button images from the resource bundle.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  pixbufs_[GTK_STATE_NORMAL] = normal_id ? rb.LoadPixbuf(normal_id) : NULL;
  pixbufs_[GTK_STATE_ACTIVE] = active_id ? rb.LoadPixbuf(active_id) : NULL;
  pixbufs_[GTK_STATE_PRELIGHT] =
      highlight_id ? rb.LoadPixbuf(highlight_id) : NULL;
  pixbufs_[GTK_STATE_SELECTED] = NULL;
  pixbufs_[GTK_STATE_INSENSITIVE] =
      depressed_id ? rb.LoadPixbuf(depressed_id) : NULL;

  gtk_widget_set_size_request(widget_,
                              gdk_pixbuf_get_width(pixbufs_[0]),
                              gdk_pixbuf_get_height(pixbufs_[0]));

  gtk_widget_set_app_paintable(widget_, TRUE);
  // We effectively double-buffer by virtue of having only one image...
  gtk_widget_set_double_buffered(widget_, FALSE);
  g_signal_connect(G_OBJECT(widget_), "expose-event",
                   G_CALLBACK(OnExpose), this);
}

CustomDrawButton::~CustomDrawButton() {
  for (size_t i = 0; i < arraysize(pixbufs_); ++i) {
    if (pixbufs_[i])
      gdk_pixbuf_unref(pixbufs_[i]);
  }
}

// static
gboolean CustomDrawButton::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                    CustomDrawButton* button) {
  GdkPixbuf* pixbuf = button->pixbufs_[GTK_WIDGET_STATE(widget)];

  // Fall back to the default image if we don't have one for this state.
  if (!pixbuf)
    pixbuf = button->pixbufs_[GTK_STATE_NORMAL];

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

CustomContainerButton::CustomContainerButton() {
  GdkPixbuf* images[9];
  int i = 0;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_TOP_LEFT_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_TOP_RIGHT_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_LEFT_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_CENTER_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_RIGHT_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_BOTTOM_LEFT_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_BOTTOM_H);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_BOTTOM_RIGHT_H);
  nine_box_prelight_.reset(new NineBox(images));

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_TOP_LEFT_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_TOP_RIGHT_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_LEFT_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_CENTER_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_RIGHT_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_BOTTOM_LEFT_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_BOTTOM_P);
  images[i++] = rb.LoadPixbuf(IDR_TEXTBUTTON_BOTTOM_RIGHT_P);
  nine_box_active_.reset(new NineBox(images));

  widget_ = gtk_button_new();
  gtk_widget_set_app_paintable(widget_, TRUE);
  g_signal_connect(G_OBJECT(widget_), "expose-event",
                   G_CALLBACK(OnExpose), this);
}

CustomContainerButton::~CustomContainerButton() {
}

// static
gboolean CustomContainerButton::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                         CustomContainerButton* button) {
  NineBox* nine_box = NULL;
  if (GTK_WIDGET_STATE(widget) == GTK_STATE_PRELIGHT)
    nine_box = button->nine_box_prelight_.get();
  else if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
    nine_box = button->nine_box_active_.get();

  // Only draw theme graphics if we have some.
  if (nine_box) {
    GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                       true,  // alpha
                                       8,  // bits per channel
                                       widget->allocation.width,
                                       widget->allocation.height);

    nine_box->RenderToPixbuf(pixbuf);

    gdk_draw_pixbuf(widget->window,
                    widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                    pixbuf,
                    0, 0,
                    widget->allocation.x, widget->allocation.y, -1, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);

    gdk_pixbuf_unref(pixbuf);
  }

  // If we return FALSE from the function, the button paints itself.
  // If we return TRUE, no children are painted.
  // So we return TRUE and send the expose along directly to the child.
  gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                 gtk_bin_get_child(GTK_BIN(widget)),
                                 e);

  return TRUE;  // Prevent normal painting.
}

