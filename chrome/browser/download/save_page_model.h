// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PAGE_MODEL_H__
#define CHROME_BROWSER_DOWNLOAD_SAVE_PAGE_MODEL_H__

#include "chrome/browser/views/download_item_view.h"

class DownloadItem;
class SavePackage;

// This class is a model class for DownloadItemView. It provides cancel
// functionality for saving page, and also the text for displaying saving
// status.
class SavePageModel : public DownloadItemView::BaseDownloadItemModel {
 public:
  SavePageModel(SavePackage* save, DownloadItem* download);
  virtual ~SavePageModel() { }

  // Cancel the page saving.
  virtual void CancelTask();

  // Get page saving status text.
  virtual std::wstring GetStatusText();

 private:
  // Saving page management.
  SavePackage* save_;

  // A fake download item for saving page use.
  DownloadItem* download_;

  DISALLOW_EVIL_CONSTRUCTORS(SavePageModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PAGE_MODEL_H__

