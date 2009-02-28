// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

#include "base/basictypes.h"

class BaseDownloadItemModel;
class TabContents;

// DownloadShelf is an interface for platform-specific download shelves to
// implement. It also contains some shared logic. This class should not be
// instantiated directly, but rather created via a call to Create().
class DownloadShelf {
 public:
  explicit DownloadShelf(TabContents* tab_contents)
      : tab_contents_(tab_contents) { }

  virtual ~DownloadShelf() { }

  // Creates a platform-specific DownloadShelf, passing ownership to the caller.
  static DownloadShelf* Create(TabContents* tab_contents);

  // A new download has started, so add it to our shelf. This object will
  // take ownership of |download_model|.
  virtual void AddDownload(BaseDownloadItemModel* download_model) = 0;

  // Invoked when the user clicks the 'show all downloads' link button.
  void ShowAllDownloads();

  // Invoked when the download shelf is migrated from one tab contents to a new
  // one.
  void ChangeTabContents(TabContents* old_contents, TabContents* new_contents);

  // The browser view needs to know when we are going away to properly return
  // the resize corner size to WebKit so that we don't draw on top of it.
  // This returns the showing state of our animation which is set to false at
  // the beginning Show and true at the beginning of a Hide.
  virtual bool IsShowing() const = 0;

 protected:
  TabContents* tab_contents_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadShelf);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

