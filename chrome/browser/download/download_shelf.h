// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

#include <string>

#include "base/logging.h"
#include "base/basictypes.h"

class BaseDownloadItemModel;
class Browser;
class DownloadItem;

// DownloadShelf is an interface for platform-specific download shelves to
// implement. It also contains some shared logic. This class should not be
// instantiated directly, but rather created via a call to Create().
// It is a view object.
class DownloadShelf {
 public:
  explicit DownloadShelf(Browser* browser)
      : browser_(browser) { DCHECK(browser_); }

  virtual ~DownloadShelf() { }

  // A new download has started, so add it to our shelf. This object will
  // take ownership of |download_model|. Also make the shelf visible.
  virtual void AddDownload(BaseDownloadItemModel* download_model) = 0;

  // Invoked when the user clicks the 'show all downloads' link button.
  void ShowAllDownloads();

  // The browser view needs to know when we are going away to properly return
  // the resize corner size to WebKit so that we don't draw on top of it.
  // This returns the showing state of our animation which is set to true at
  // the beginning Show and false at the beginning of a Hide.
  virtual bool IsShowing() const = 0;

  // Returns whether the download shelf is showing the close animation.
  virtual bool IsClosing() const = 0;

  // Opens the shelf.
  virtual void Show() = 0;

  // Closes the shelf.
  virtual void Close() = 0;

  Browser* browser() { return browser_; }

 protected:
  Browser* browser_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadShelf);
};

// Logic for the download shelf context menu. Platform specific subclasses are
// responsible for creating and running the menu.
class DownloadShelfContextMenu {
 public:
  virtual ~DownloadShelfContextMenu();

 protected:
  explicit DownloadShelfContextMenu(BaseDownloadItemModel* download_model);

  enum ContextMenuCommands {
    SHOW_IN_FOLDER = 1,  // Open a file explorer window with the item selected
    OPEN_WHEN_COMPLETE,  // Open the download when it's finished
    ALWAYS_OPEN_TYPE,    // Default this file extension to always open
    CANCEL,              // Cancel the download
    MENU_LAST
  };

 protected:
  bool ItemIsChecked(int id) const;
  bool ItemIsDefault(int id) const;
  std::wstring GetItemLabel(int id) const;
  bool IsItemCommandEnabled(int id) const;
  void ExecuteItemCommand(int id);

  // Information source.
  DownloadItem* download_;

  // A model to control the cancel behavior.
  BaseDownloadItemModel* model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadShelfContextMenu);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
