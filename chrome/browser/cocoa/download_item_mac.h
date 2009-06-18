// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_DOWNLOAD_ITEM_MAC_H_
#define CHROME_BROWSER_COCOA_DOWNLOAD_ITEM_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_manager.h"

class BaseDownloadItemModel;
@class DownloadShelfController;
@class DownloadShelfContextMenuBridge;


// A class that bridges the visible mac download items to chromium's
// download model.

class DownloadItemMac : DownloadItem::Observer {
 public:
  // DownloadItemMac takes ownership of |download_item_model|.
  DownloadItemMac(BaseDownloadItemModel* download_item_model,
                  NSRect frame,
                  DownloadShelfController* parent);

  // Destructor.
  ~DownloadItemMac();

  // DownloadItem::Observer implementation
  virtual void OnDownloadUpdated(DownloadItem* download);
  virtual void OnDownloadOpened(DownloadItem* download) { }

 private:
  // The download item model we represent.
  scoped_ptr<BaseDownloadItemModel> download_model_;

  // Our parent view
  DownloadShelfController* parent_;  // weak

  // Context menu
  scoped_nsobject<DownloadShelfContextMenuBridge> menu_;
};

#endif  // CHROME_BROWSER_COCOA_DOWNLOAD_ITEM_MAC_H_
