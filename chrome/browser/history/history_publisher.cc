// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_publisher.h"

namespace history {

const char* HistoryPublisher::kThumbnailImageFormat = "image/jpeg";

void HistoryPublisher::PublishPageThumbnail(
    const std::vector<unsigned char>& thumbnail, const GURL& url,
    const base::Time& time) const {
  PageData page_data = {
    time,
    url,
    NULL,
    NULL,
    kThumbnailImageFormat,
    &thumbnail,
  };

  PublishDataToIndexers(page_data);
}

void HistoryPublisher::PublishPageContent(const base::Time& time,
                                          const GURL& url,
                                          const std::wstring& title,
                                          const std::wstring& contents) const {
  PageData page_data = {
    time,
    url,
    contents.c_str(),
    title.c_str(),
    NULL,
    NULL,
  };

  PublishDataToIndexers(page_data);
}

}  // namespace history
