// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Support modularity by calling to load a new SDCH filter dictionary.
// Note that this sort of calling can't be done in the /net directory, as it has
// no concept of the HTTP cache (which is only visible at the browser level).

#ifndef CHROME_BROWSER_NET_SDCH_DICTIONARY_FETCHER_H_
#define CHROME_BROWSER_NET_SDCH_DICTIONARY_FETCHER_H_

#include <queue>
#include <string>

#include "base/compiler_specific.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/net/url_fetcher.h"
#include "net/base/sdch_manager.h"

class SdchDictionaryFetcher : public URLFetcher::Delegate,
                              public SdchFetcher {
 public:
  SdchDictionaryFetcher() :
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      task_is_pending_(false) {}
  virtual ~SdchDictionaryFetcher() {}

  // Implementation of SdchFetcher class.
  // This method gets the requested dictionary, and then calls back into the
  // SdchManager class with the dictionary's text.
  virtual void Schedule(const GURL& dictionary_url);

 private:
  // Delay in ms between Schedule and actual download.
  // This leaves the URL in a queue, which is de-duped, so that there is less
  // chance we'll try to load the same URL multiple times when a pile of
  // page subresources (or tabs opened in parallel) all suggest the dictionary.
  static const int kMsDelayFromRequestTillDownload = 100;

  // Ensure the download after the above delay.
  void ScheduleDelayedRun();

  // Make sure we're processing (or waiting for) the the arrival of the next URL
  // in the  |fetch_queue_|.
  void StartFetching();

  // Implementation of URLFetcher::Delegate. Called after transmission
  // completes (either successfully or with failure).
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // A queue of URLs that are being used to download dictionaries.
  std::queue<GURL> fetch_queue_;
  // The currently outstanding URL fetch of a dicitonary.
  // If this is null, then there is no outstanding request.
  scoped_ptr<URLFetcher> current_fetch_;

  // Always spread out the dictionary fetches, so that they don't steal
  // bandwidth from the actual page load.  Create delayed tasks to spread out
  // the download.
  ScopedRunnableMethodFactory<SdchDictionaryFetcher> method_factory_;
  bool task_is_pending_;

  DISALLOW_COPY_AND_ASSIGN(SdchDictionaryFetcher);
};

#endif  // CHROME_BROWSER_NET_SDCH_DICTIONARY_FETCHER_H_
