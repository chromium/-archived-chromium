// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_DOWNLOAD_SHELF_GTK_H_
#define CHROME_BROWSER_GTK_DOWNLOAD_SHELF_GTK_H_

#include <gtk/gtk.h>

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/common/owned_widget_gtk.h"

class BaseDownloadItemModel;
class Browser;
class CustomDrawButton;
class DownloadItemGtk;
class GtkThemeProperties;
class SlideAnimatorGtk;

class DownloadShelfGtk : public DownloadShelf {
 public:
  explicit DownloadShelfGtk(Browser* browser, gfx::NativeView view);

  ~DownloadShelfGtk();

  // DownloadShelf implementation.
  virtual void AddDownload(BaseDownloadItemModel* download_model);
  virtual bool IsShowing() const;
  virtual bool IsClosing() const;
  virtual void Show();
  virtual void Close();

  // Returns the current height of the shelf.
  int GetHeight() const;

  // Changes the color of the background shelf.
  void UserChangedTheme(GtkThemeProperties* properties);

 private:
  // Remove |download_item| from the download shelf and delete it.
  void RemoveDownloadItem(DownloadItemGtk* download_item);

  // Get the leftmost non-download item widget on the shelf.
  GtkWidget* GetRightBoundingWidget() const;

  // Get the hbox download items ought to pack themselves into.
  GtkWidget* GetHBox() const;

  static void OnButtonClick(GtkWidget* button, DownloadShelfGtk* toolbar);

  // The top level widget of the shelf.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // |hbox_| holds the download items and buttons of the shelf.
  OwnedWidgetGtk hbox_;

  // |shelf_| is the second highest level widget. See the constructor
  // for an explanation of the widget layout.
  OwnedWidgetGtk shelf_;

  // A GtkEventBox which we color.
  GtkWidget* padding_bg_;

  // This hbox holds the link text and download icon. It also holds the
  // distinction of being the leftmost non-download item widget on the shelf.
  GtkWidget* link_hbox_;

  // The 'x' that the user can press to hide the download shelf.
  scoped_ptr<CustomDrawButton> close_button_;

  // Keeps track of our current hide/show state.
  bool is_showing_;

  // The download items we have added to our shelf.
  std::vector<DownloadItemGtk*> download_items_;

  friend class DownloadItemGtk;
};

#endif  // CHROME_BROWSER_GTK_DOWNLOAD_SHELF_GTK_H_
