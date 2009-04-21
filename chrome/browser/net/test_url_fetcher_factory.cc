// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/test_url_fetcher_factory.h"

TestURLFetcher::TestURLFetcher(const GURL& url,
                               URLFetcher::RequestType request_type,
                               URLFetcher::Delegate* d)
    : URLFetcher(url, request_type, d),
      original_url_(url) {
}

URLFetcher* TestURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* d) {
  TestURLFetcher* fetcher = new TestURLFetcher(url, request_type, d);
  fetchers_[id] = fetcher;
  // URLFetcher's destructor requires the message loop.
  fetcher->set_io_loop(MessageLoop::current());
  return fetcher;
}

TestURLFetcher* TestURLFetcherFactory::GetFetcherByID(int id) const {
  Fetchers::const_iterator i = fetchers_.find(id);
  return i == fetchers_.end() ? NULL : i->second;
}
