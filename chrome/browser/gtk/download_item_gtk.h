// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DOWNLOAD_ITEM_GTK_H_
#define CHROME_BROWSER_GTK_DOWNLOAD_ITEM_GTK_H_

#include <gtk/gtk.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_manager.h"

class BaseDownloadItemModel;
class DownloadShelfContextMenuGtk;
class NineBox;

class DownloadItemGtk : DownloadItem::Observer {
 public:
  // DownloadItemGtk takes ownership of |download_item_model|.
  DownloadItemGtk(BaseDownloadItemModel* download_item_model,
                  GtkWidget* parent_shelf, GtkWidget* bounding_widget);

  // We put |hbox_| in |parent_shelf| and rely on |parent_shelf| recursively
  // destroying its children. Hence we do nothing in the destructor.
  ~DownloadItemGtk();

  // DownloadItem::Observer implementation
  virtual void OnDownloadUpdated(DownloadItem* download);

 private:
  static void InitNineBoxes();

  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           DownloadItemGtk* download_item);

  static gboolean OnMenuButtonPressEvent(GtkWidget* button,
                                         GdkEvent* event,
                                         DownloadItemGtk* item);

  static void OnShelfResized(GtkWidget *widget,
                             GtkAllocation *allocation,
                             DownloadItemGtk* item);

  // Nineboxes for the body area.
  static NineBox* body_nine_box_normal_;
  static NineBox* body_nine_box_prelight_;
  static NineBox* body_nine_box_active_;

  // Nineboxes for the menu button.
  static NineBox* menu_nine_box_normal_;
  static NineBox* menu_nine_box_prelight_;
  static NineBox* menu_nine_box_active_;

  // The widget that contains the body and menu dropdown.
  GtkWidget* hbox_;

  // The widget that contains the name of the download and the progress
  // animation.
  GtkWidget* body_;

  // The GtkLabel that holds the status text.
  GtkWidget* status_label_;

  // The widget that creates a dropdown menu when pressed.
  GtkWidget* menu_button_;

  // The menu that pops down when the user presses |menu_button_|. We do not
  // create this until the first time we actually need it.
  scoped_ptr<DownloadShelfContextMenuGtk> menu_;

  // The download item model we represent.
  scoped_ptr<BaseDownloadItemModel> download_model_;

  // The shelf we show ourselves on. We do not own this widget.
  GtkWidget* parent_shelf_;

  // This is the leftmost widget on |parent_shelf_| that is not a download item.
  // We do not want to overlap it.
  GtkWidget* bounding_widget_;
};

#endif  // CHROME_BROWSER_GTK_DOWNLOAD_ITEM_GTK_H_
