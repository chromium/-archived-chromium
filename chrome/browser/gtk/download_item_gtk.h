// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DOWNLOAD_ITEM_GTK_H_
#define CHROME_BROWSER_GTK_DOWNLOAD_ITEM_GTK_H_

#include <gtk/gtk.h>

#include "app/animation.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/icon_manager.h"

class BaseDownloadItemModel;
class DownloadShelfContextMenuGtk;
class DownloadShelfGtk;
class NineBox;
class SkBitmap;
class SlideAnimation;

class DownloadItemGtk : public DownloadItem::Observer,
                        public AnimationDelegate {
 public:
  // DownloadItemGtk takes ownership of |download_item_model|.
  DownloadItemGtk(DownloadShelfGtk* parent_shelf,
                  BaseDownloadItemModel* download_item_model);

  // We put |hbox_| in |parent_shelf| and rely on |parent_shelf| recursively
  // destroying its children. Hence we do nothing in the destructor.
  ~DownloadItemGtk();

  // DownloadItem::Observer implementation.
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download) { }

  // AnimationDelegate implementation.
  virtual void AnimationProgressed(const Animation* animation);

  // Called when the icon manager has finished loading the icon. We take
  // ownership of |icon_bitmap|.
  void OnLoadIconComplete(IconManager::Handle handle, SkBitmap* icon_bitmap);

 private:
  friend class DownloadShelfContextMenuGtk;

  // Functions for controlling the progress animation.
  // Repaint the download progress.
  void UpdateDownloadProgress();

  // Starts a repeating timer for UpdateDownloadProgress.
  void StartDownloadProgress();

  // Stops the repeating timer.
  void StopDownloadProgress();

  // Ask the icon manager to asynchronously start loading the icon for the file.
  void LoadIcon();

  static void InitNineBoxes();

  // Used for the download item's body and menu button.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           DownloadItemGtk* download_item);

  // Used for the download icon.
  static gboolean OnProgressAreaExpose(GtkWidget* widget,
                                       GdkEventExpose* e,
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

  // The shelf on which we are displayed.
  DownloadShelfGtk* parent_shelf_;

  // The widget that contains the body and menu dropdown.
  GtkWidget* hbox_;

  // The widget that contains the name of the download and the progress
  // animation.
  GtkWidget* body_;

  // The GtkLabel that holds the download title text.
  GtkWidget* name_label_;

  // The GtkLabel that holds the status text.
  GtkWidget* status_label_;

  // The widget that creates a dropdown menu when pressed.
  GtkWidget* menu_button_;

  // Whether the menu is currently showing for |menu_button_|. Affects how we
  // draw the button.
  bool menu_showing_;

  // The widget that contains the animation progress and the file's icon
  // (as well as the complete animation).
  GtkWidget* progress_area_;

  // In degrees. Only used for downloads with no known total size.
  int progress_angle_;

  // The menu that pops down when the user presses |menu_button_|. We do not
  // create this until the first time we actually need it.
  scoped_ptr<DownloadShelfContextMenuGtk> menu_;

  // The download item model we represent.
  scoped_ptr<BaseDownloadItemModel> download_model_;

  // This is the leftmost widget on |parent_shelf_| that is not a download item.
  // We do not want to overlap it.
  GtkWidget* bounding_widget_;

  // The ID of the handler for the parent shelf's "size-allocate" event. We save
  // it so we can disconnect when we are destroyed.
  gulong resize_handler_id_;

  // The animation when this item is first added to the shelf.
  scoped_ptr<SlideAnimation> new_item_animation_;

  // Progress animation.
  base::RepeatingTimer<DownloadItemGtk> progress_timer_;

  // Animation for download complete.
  scoped_ptr<SlideAnimation> complete_animation_;

  // The file icon for the download. May be null.
  SkBitmap* icon_;

  // For canceling an in progress icon request.
  CancelableRequestConsumerT<int, 0> icon_consumer_;
};

#endif  // CHROME_BROWSER_GTK_DOWNLOAD_ITEM_GTK_H_
