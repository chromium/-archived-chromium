// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H__
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H__

#include "chrome/browser/views/download_item_view.h"

class DownloadItem;

// This class is a model class for DownloadItemView. It provides functionality
// for canceling the downloading, and also the text for displaying downloading
// status.
class DownloadItemModel : public DownloadItemView::BaseDownloadItemModel {
 public:
  DownloadItemModel(DownloadItem* download);
  virtual ~DownloadItemModel() { }

  // Cancel the downloading.
  virtual void CancelTask();

  // Get downloading status text.
  virtual std::wstring GetStatusText();

 private:
  // We query this item for status information.
  DownloadItem* download_;

  DISALLOW_EVIL_CONSTRUCTORS(DownloadItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H__

