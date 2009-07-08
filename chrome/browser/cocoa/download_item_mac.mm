// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/download_item_mac.h"

#import "chrome/browser/cocoa/download_item_controller.h"
#include "chrome/browser/download/download_item_model.h"

// DownloadItemMac -------------------------------------------------------------

DownloadItemMac::DownloadItemMac(BaseDownloadItemModel* download_model,
                                 DownloadItemController* controller)
    : download_model_(download_model), item_controller_(controller) {
  download_model_->download()->AddObserver(this);
}

DownloadItemMac::~DownloadItemMac() {
  download_model_->download()->RemoveObserver(this);
}

void DownloadItemMac::OnDownloadUpdated(DownloadItem* download) {
  DCHECK_EQ(download, download_model_->download());

  [item_controller_ setStateFromDownload:download_model_.get()];
}
