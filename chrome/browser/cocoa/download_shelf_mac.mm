// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/download_shelf_mac.h"

#import "chrome/browser/cocoa/download_shelf_controller.h"
#include "chrome/browser/cocoa/download_item_mac.h"
#include "chrome/browser/download/download_item_model.h"

DownloadShelfMac::DownloadShelfMac(Browser* browser,
                                   DownloadShelfController* controller)
    : DownloadShelf(browser),
      shelf_controller_(controller) {
  Show();
}

void DownloadShelfMac::AddDownload(BaseDownloadItemModel* download_model) {
  [shelf_controller_ addDownloadItem:download_model];
  Show();
}

bool DownloadShelfMac::IsShowing() const {
  return [shelf_controller_ isVisible] == YES;
}

bool DownloadShelfMac::IsClosing() const {
  // TODO(estade): This is never called. For now just return false.
  return false;
}

void DownloadShelfMac::Show() {
  [shelf_controller_ show:nil];
}

void DownloadShelfMac::Close() {
  [shelf_controller_ hide:nil];
}
