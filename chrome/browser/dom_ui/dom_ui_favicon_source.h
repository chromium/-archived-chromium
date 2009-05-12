// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_FAVICON_SOURCE_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_FAVICON_SOURCE_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/history/history.h"

class GURL;
class Profile;

// FavIconSource is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
class DOMUIFavIconSource : public ChromeURLDataManager::DataSource {
 public:
  explicit DOMUIFavIconSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    // We need to explicitly return a mime type, otherwise if the user tries to
    // drag the image they get no extension.
    return "image/png";
  }

  // Called when favicon data is available from the history backend.
  void OnFavIconDataAvailable(HistoryService::Handle request_handle,
                              bool know_favicon,
                              scoped_refptr<RefCountedBytes> data,
                              bool expired,
                              GURL url);

 private:
  Profile* profile_;
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  // Raw PNG representation of the favicon to show when the favicon
  // database doesn't have a favicon for a webpage.
  scoped_refptr<RefCountedBytes> default_favicon_;

  DISALLOW_COPY_AND_ASSIGN(DOMUIFavIconSource);
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_FAVICON_SOURCE_H_
