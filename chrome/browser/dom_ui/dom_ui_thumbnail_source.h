// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_THUMBNAIL_SOURCE_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_THUMBNAIL_SOURCE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/history/history.h"

class Profile;
class ThumbnailStore;

// ThumbnailSource is the gateway between network-level chrome:
// requests for thumbnails and the history backend that serves these.
class DOMUIThumbnailSource : public ChromeURLDataManager::DataSource {
 public:
  explicit DOMUIThumbnailSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    // We need to explicitly return a mime type, otherwise if the user tries to
    // drag the image they get no extension.
    return "image/png";
  }

  // Called when thumbnail data is available from the history backend.
  void OnThumbnailDataAvailable(HistoryService::Handle request_handle,
                                scoped_refptr<RefCountedBytes> data);

 private:
  Profile* profile_;
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  // The ThumbnailStore from which thumbnails are requested.
  scoped_refptr<ThumbnailStore> store_;

  // Raw PNG representation of the thumbnail to show when the thumbnail
  // database doesn't have a thumbnail for a webpage.
  scoped_refptr<RefCountedBytes> default_thumbnail_;

  DISALLOW_COPY_AND_ASSIGN(DOMUIThumbnailSource);
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_THUMBNAIL_SOURCE_H_
