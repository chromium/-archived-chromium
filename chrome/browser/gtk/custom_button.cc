// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/custom_button.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/common/resource_bundle.h"

CustomDrawButton::CustomDrawButton(int normal_id,
    int active_id, int highlight_id, int depressed_id) {
  widget_ = gtk_button_new();

  // Load the button images from the theme resources .pak file.
  pixbufs_[GTK_STATE_NORMAL] = LoadImage(normal_id);
  pixbufs_[GTK_STATE_ACTIVE] = LoadImage(active_id);
  pixbufs_[GTK_STATE_PRELIGHT] = LoadImage(highlight_id);
  pixbufs_[GTK_STATE_SELECTED] = NULL;
  pixbufs_[GTK_STATE_INSENSITIVE] = LoadImage(depressed_id);

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

GdkPixbuf* CustomDrawButton::LoadImage(int resource_id) {
  if (0 == resource_id)
    return NULL;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  std::vector<unsigned char> data;
  rb.LoadImageResourceBytes(resource_id, &data);

  GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
  bool ok = gdk_pixbuf_loader_write(loader, static_cast<guint8*>(data.data()),
      data.size(), NULL);
  DCHECK(ok) << "failed to write " << resource_id;
  // Calling gdk_pixbuf_loader_close forces the data to be parsed by the
  // loader.  We must do this before calling gdk_pixbuf_loader_get_pixbuf.
  ok = gdk_pixbuf_loader_close(loader, NULL);
  DCHECK(ok) << "close failed " << resource_id;
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
  DCHECK(pixbuf) << "failed to load " << resource_id << " " << data.size();

  // The pixbuf is owned by the loader, so add a ref so when we delete the
  // loader, the pixbuf still exists.
  g_object_ref(pixbuf);
  g_object_unref(loader);

  return pixbuf;
}

// static
gboolean CustomDrawButton::OnExpose(
    GtkWidget* widget,
    GdkEventExpose* e,
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
