// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_
#define CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_

#include <gtk/gtk.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/custom_button.h"

class BaseDownloadItemModel;

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

  // |bar_| is the highest level widget of the download shelf. It is an hbox.
  GtkWidget* shelf_;

  // The 'x' that the user can press to hide the download shelf.
  scoped_ptr<CustomDrawButton> close_button_;

  // Keeps track of our current hide/show state.
  bool is_showing_;
};

#endif  // CHROME_BROWSER_VIEWS_DOWNLOAD_SHELF_VIEW_H_

