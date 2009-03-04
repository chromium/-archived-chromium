// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_item_gtk.h"

#include "base/basictypes.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/common/resource_bundle.h"

#include "grit/theme_resources.h"

NineBox* DownloadItemGtk::nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::nine_box_active_ = NULL;

DownloadItemGtk::DownloadItemGtk(BaseDownloadItemModel* download_model,
                                 GtkWidget* parent_shelf)
    : download_model_(download_model),
      parent_shelf_(parent_shelf) {
  InitNineBoxes();

  body_ = gtk_button_new();
  gtk_widget_set_app_paintable(body_, TRUE);
  g_signal_connect(G_OBJECT(body_), "expose-event",
                   G_CALLBACK(OnBodyExpose), this);

  GtkWidget* label = gtk_label_new(download_model->download()->file_name()
      .value().c_str());
  gtk_container_add(GTK_CONTAINER(body_), label);

  hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), body_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(parent_shelf), hbox_, FALSE, FALSE, 0);
  gtk_widget_show_all(hbox_);
}

// static
void DownloadItemGtk::InitNineBoxes() {
  if (nine_box_normal_)
    return;

  GdkPixbuf* images[9];
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  int i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM);
  nine_box_normal_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H);
  nine_box_prelight_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P);
  nine_box_active_ = new NineBox(images);
}

// static
gboolean DownloadItemGtk::OnBodyExpose(GtkWidget* widget, GdkEventExpose* e,
                                       DownloadItemGtk* download_item) {
  NineBox* nine_box = NULL;
  if (GTK_WIDGET_STATE(widget) == GTK_STATE_PRELIGHT)
    nine_box = nine_box_prelight_;
  else if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
    nine_box = nine_box_active_;
  else
    nine_box = nine_box_normal_;

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

  gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                 gtk_bin_get_child(GTK_BIN(widget)),
                                 e);

  return TRUE;
}


