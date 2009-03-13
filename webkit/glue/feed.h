// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_COMMON_FEED_H_
#define CHROME_COMMON_FEED_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"

struct FeedItem {
  // The feed title.
  std::wstring title;
  // The feed type, for example: "application/rss+xml". The type can be blank.
  std::wstring type;
  // The URL to subscribe to the feed.
  GURL url;
};

class FeedList : public base::RefCounted<FeedList> {
 public:
  // We limit the number of feeds that can be sent so that a rouge renderer
  // doesn't cause excessive memory usage in the browser process by specifying
  // a huge number of RSS feeds for the browser to parse.
  static const size_t kMaxFeeds = 50;

  FeedList() {}

  void Add(const FeedItem &item) {
    list_.push_back(item);
  }

  const std::vector<FeedItem>& list() const {
    return list_;
  }

 private:
  std::vector<FeedItem> list_;

  DISALLOW_COPY_AND_ASSIGN(FeedList);
};

#endif  // CHROME_COMMON_FEED_H_
