// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_
#define CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_

#include <gtk/gtk.h>

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/custom_button.h"

class BaseDownloadItemModel;
class DownloadItemGtk;

class DownloadShelfGtk : public DownloadShelf {
 public:
  explicit DownloadShelfGtk(TabContents* tab_contents);

  // DownloadShelf implementation.
  virtual void AddDownload(BaseDownloadItemModel* download_model);
  virtual bool IsShowing() const;

 private:
  // Show the shelf.
  void Show();

  // Hide the shelf.
  void Hide();

  static void OnCloseButtonClick(GtkWidget* button,
                                 DownloadShelfGtk* toolbar);

  // |hbox_| holds the download items and buttons of the shelf.
  GtkWidget* hbox_;

  // |shelf_box_| is the highest level widget of the shelf. It has a single
  // child, |hbox_|. It exists because we need to be able to set the background
  // color of the shelf, and GtkBoxes don't have a gdk window, which is a
  // requisite for changing the background color.
  GtkWidget* shelf_;

  // The 'x' that the user can press to hide the download shelf.
  scoped_ptr<CustomDrawButton> close_button_;

  // Keeps track of our current hide/show state.
  bool is_showing_;

  // The download items we have added to our shelf.
  std::vector<DownloadItemGtk*> download_items_;
};

#endif  // CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_
