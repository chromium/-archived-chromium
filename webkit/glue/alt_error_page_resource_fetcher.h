// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__
#define WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__

#include <string>

#include "base/scoped_ptr.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/resource_fetcher.h"

class ResourceFetcherWithTimeout;
class WebFrameImpl;
class WebView;

namespace webkit_glue {

// Used for downloading alternate dns error pages. Once downloading is done
// (or fails), the webview delegate is notified.
class AltErrorPageResourceFetcher {
 public:
  AltErrorPageResourceFetcher(WebView* web_view,
                              WebFrame* web_frame,
                              const WebKit::WebURLError& web_error,
                              const GURL& url);
  ~AltErrorPageResourceFetcher();

 private:
  void OnURLFetchComplete(const WebKit::WebURLResponse& response,
                          const std::string& data);

  // References to our owners
  WebView* web_view_;
  WebFrame* web_frame_;
  WebKit::WebURLError web_error_;
  WebKit::WebURLRequest failed_request_;

  // Does the actual fetching.
  scoped_ptr<ResourceFetcherWithTimeout> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AltErrorPageResourceFetcher);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__
