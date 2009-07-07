// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_ALT_404_PAGE_RESOURCE_HANDLE_CLIENT_H_
#define WEBKIT_GLUE_ALT_404_PAGE_RESOURCE_HANDLE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

#include "webkit/glue/resource_fetcher.h"

class WebFrameLoaderClient;

namespace webkit_glue {

// ResourceHandleClient implementation that is used for downloading alternate
// 404 pages. Once downloading is done (or fails), the WebFrameLoaderClient is
// notified.
class Alt404PageResourceFetcher {
 public:
  Alt404PageResourceFetcher(WebFrameLoaderClient* webframeloaderclient,
                            WebCore::Frame* frame,
                            WebCore::DocumentLoader* doc_loader,
                            const GURL& url);

  // Stop any pending loads.
  void Cancel() {
    if (fetcher_.get())
      fetcher_->Cancel();
  }

 private:
  void OnURLFetchComplete(const WebKit::WebURLResponse& response,
                          const std::string& data);

  // Does the actual fetching.
  scoped_ptr<ResourceFetcherWithTimeout> fetcher_;

  // References to our owner which we call when finished.
  WebFrameLoaderClient* webframeloaderclient_;

  // The DocumentLoader associated with this load.  If there's an error
  // talking with the alt 404 page server, we need this to complete the
  // original load.
  RefPtr<WebCore::DocumentLoader> doc_loader_;

  DISALLOW_COPY_AND_ASSIGN(Alt404PageResourceFetcher);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_ALT_404_PAGE_RESOURCE_HANDLE_CLIENT_H_
