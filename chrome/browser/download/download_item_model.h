// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_

#include <string>

#include "base/basictypes.h"

class DownloadItem;
class SavePackage;

// This class provides an interface for functions which have different behaviors
// depending on the type of download.
class BaseDownloadItemModel {
 public:
  BaseDownloadItemModel(DownloadItem* download) : download_(download) { }
  virtual ~BaseDownloadItemModel() { }

  // Cancel the task corresponding to the item.
  virtual void CancelTask() = 0;

  // Get the status text to display.
  virtual std::wstring GetStatusText() = 0;

  DownloadItem* download() { return download_; }

 protected:
  DownloadItem* download_;
};

// This class is a model class for DownloadItemView. It provides functionality
// for canceling the downloading, and also the text for displaying downloading
// status.
class DownloadItemModel : public BaseDownloadItemModel {
 public:
  DownloadItemModel(DownloadItem* download);
  virtual ~DownloadItemModel() { }

  // Cancel the downloading.
  virtual void CancelTask();

  // Get downloading status text.
  virtual std::wstring GetStatusText();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadItemModel);
};

// This class is a model class for DownloadItemView. It provides cancel
// functionality for saving page, and also the text for displaying saving
// status.
class SavePageModel : public BaseDownloadItemModel {
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

  DISALLOW_COPY_AND_ASSIGN(SavePageModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
