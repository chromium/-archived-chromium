// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_ALT_404_PAGE_RESOURCE_HANDLE_CLIENT_H__
#define WEBKIT_GLUE_ALT_404_PAGE_RESOURCE_HANDLE_CLIENT_H__

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

#include "webkit/glue/resource_fetcher.h"

class WebFrameLoaderClient;

// ResourceHandleClient implementation that is used for downloading alternate
// 404 pages. Once downloading is done (or fails), the WebFrameLoaderClient is
// notified.
class Alt404PageResourceFetcher : public ResourceFetcher::Delegate {
 public:
  Alt404PageResourceFetcher(WebFrameLoaderClient* webframeloaderclient,
                            WebCore::Frame* frame,
                            WebCore::DocumentLoader* doc_loader,
                            const GURL& url);

  virtual void OnURLFetchComplete(const WebCore::ResourceResponse& response,
                                  const std::string& data);

  // Stop any pending loads.
  void Cancel() {
    if (fetcher_.get())
      fetcher_->Cancel();
  }

 private:
  // Does the actual fetching.
  scoped_ptr<ResourceFetcherWithTimeout> fetcher_;

  // References to our owner which we call when finished.
  WebFrameLoaderClient* webframeloaderclient_;

  // The DocumentLoader associated with this load.  If there's an error
  // talking with the alt 404 page server, we need this to complete the
  // original load.
  RefPtr<WebCore::DocumentLoader> doc_loader_;

  DISALLOW_EVIL_CONSTRUCTORS(Alt404PageResourceFetcher);
};

#endif  // WEBKIT_GLUE_ALT_404_PAGE_RESOURCE_HANDLE_CLIENT_H__
