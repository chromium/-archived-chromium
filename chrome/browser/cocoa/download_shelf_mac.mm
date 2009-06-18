// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/download_shelf_mac.h"

#import "chrome/browser/cocoa/download_shelf_controller.h"
#include "chrome/browser/cocoa/download_item_mac.h"
#include "chrome/browser/download/download_item_model.h"

namespace {

// TODO(thakis): These are all temporary until there's a download item view

// Border padding of a download item
const int kDownloadItemBorderPadding = 4;

// Width of a download item
const int kDownloadItemWidth = 200;

// Height of a download item
const int kDownloadItemHeight = 32;

// Horizontal padding between two download items
const int kDownloadItemPadding = 10;

}  // namespace

DownloadShelfMac::DownloadShelfMac(Browser* browser,
                                   DownloadShelfController* controller)
    : DownloadShelf(browser),
      shelf_controller_(controller) {
  Show();
}

void DownloadShelfMac::AddDownload(BaseDownloadItemModel* download_model) {

  // TODO(thakis): we need to delete these at some point. There's no explicit
  // mass delete on windows, figure out where they do it.

  // TODO(thakis): This should just forward to the controller.

  // TODO(thakis): RTL support?
  int startX = kDownloadItemBorderPadding +
      (kDownloadItemWidth + kDownloadItemPadding) * download_items_.size();
  download_items_.push_back(new DownloadItemMac(download_model,
      NSMakeRect(startX, kDownloadItemBorderPadding,
                 kDownloadItemWidth, kDownloadItemHeight),
      shelf_controller_));

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
