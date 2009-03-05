// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_FILEICON_SOURCE_H_
#define CHROME_BROWSER_DOM_UI_FILEICON_SOURCE_H_

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/resource_bundle.h"

#if defined(OS_WIN)
#include "chrome/browser/icon_manager.h"
#else
// TODO(port): Remove when IconManager has been ported.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class GURL;

// FileIconSource is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
class FileIconSource : public ChromeURLDataManager::DataSource {
 public:
  explicit FileIconSource();
  virtual ~FileIconSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    // Rely on image decoder inferring the correct type.
    return std::string();
  }

  // Called when favicon data is available from the history backend.
  void OnFileIconDataAvailable(
      IconManager::Handle request_handle,
      SkBitmap* icon);

 private:
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  // Raw PNG representation of the favicon to show when the favicon
  // database doesn't have a favicon for a webpage.
  scoped_refptr<RefCountedBytes> default_favicon_;

  DISALLOW_COPY_AND_ASSIGN(FileIconSource);
};
#endif  // CHROME_BROWSER_DOM_UI_FILEICON_SOURCE_H_

