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
#include "chrome/common/owned_widget_gtk.h"

class BaseDownloadItemModel;
class DownloadShelfContextMenuGtk;
class DownloadShelfGtk;
class GtkThemeProperties;
class NineBox;
class SkBitmap;
class SlideAnimation;

class DownloadItemGtk : public DownloadItem::Observer,
                        public AnimationDelegate {
 public:
  // DownloadItemGtk takes ownership of |download_item_model|.
  DownloadItemGtk(DownloadShelfGtk* parent_shelf,
                  BaseDownloadItemModel* download_item_model);

  // Destroys all widgets belonging to this DownloadItemGtk.
  ~DownloadItemGtk();

  // DownloadItem::Observer implementation.
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download) { }

  // AnimationDelegate implementation.
  virtual void AnimationProgressed(const Animation* animation);

  // Changes the color of the background shelf.
  void UserChangedTheme(GtkThemeProperties* properties);

  // Called when the icon manager has finished loading the icon. We take
  // ownership of |icon_bitmap|.
  void OnLoadIconComplete(IconManager::Handle handle, SkBitmap* icon_bitmap);

 private:
  friend class DownloadShelfContextMenuGtk;

  DownloadItem* get_download();

  // Returns true IFF the download is dangerous and unconfirmed.
  bool IsDangerous();

  // Functions for controlling the progress animation.
  // Repaint the download progress.
  void UpdateDownloadProgress();

  // Starts a repeating timer for UpdateDownloadProgress.
  void StartDownloadProgress();

  // Stops the repeating timer.
  void StopDownloadProgress();

  // Ask the icon manager to asynchronously start loading the icon for the file.
  void LoadIcon();

  // Sets the name label to the correct color.
  void UpdateNameLabel();

  // Sets the text with the correct color if |status_label| exists.
  void UpdateStatusLabel(GtkWidget* status_label,
                         const std::string& status_text);

  static void InitNineBoxes();

  // Used for the download item's body and menu button.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e,
                           DownloadItemGtk* download_item);

  // Called when |body_| is clicked.
  static void OnClick(GtkWidget* widget, DownloadItemGtk* item);

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

  // Dangerous download related. -----------------------------------------------
  static gboolean OnDangerousPromptExpose(GtkWidget* widget,
                                         GdkEventExpose* event,
                                         DownloadItemGtk* item);

  static void OnDangerousAccept(GtkWidget* button, DownloadItemGtk* item);
  static void OnDangerousDecline(GtkWidget* button, DownloadItemGtk* item);

  // Nineboxes for the body area.
  static NineBox* body_nine_box_normal_;
  static NineBox* body_nine_box_prelight_;
  static NineBox* body_nine_box_active_;

  // Nineboxes for the menu button.
  static NineBox* menu_nine_box_normal_;
  static NineBox* menu_nine_box_prelight_;
  static NineBox* menu_nine_box_active_;

  // Ninebox for the background of the dangerous download prompt.
  static NineBox* dangerous_nine_box_;

  // The shelf on which we are displayed.
  DownloadShelfGtk* parent_shelf_;

  // The widget that contains the body and menu dropdown.
  OwnedWidgetGtk hbox_;

  // The widget that contains the name of the download and the progress
  // animation.
  OwnedWidgetGtk body_;

  // The GtkLabel that holds the download title text.
  GtkWidget* name_label_;

  // The GtkLabel that holds the status text.
  GtkWidget* status_label_;

  // The current text of status label
  std::string status_text_;

  // The widget that creates a dropdown menu when pressed.
  GtkWidget* menu_button_;

  // Whether the menu is currently showing for |menu_button_|. Affects how we
  // draw the button.
  bool menu_showing_;

  // Whether we should use the GTK text color
  bool use_gtk_colors_;

  // The widget that contains the animation progress and the file's icon
  // (as well as the complete animation).
  OwnedWidgetGtk progress_area_;

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

  // The dangerous download dialog. This will be null for safe downloads.
  GtkWidget* dangerous_prompt_;

  // An hbox for holding components of the dangerous download dialog.
  GtkWidget* dangerous_hbox_;
  int dangerous_hbox_start_width_;
  int dangerous_hbox_full_width_;

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
