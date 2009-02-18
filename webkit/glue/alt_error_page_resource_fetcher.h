// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__
#define WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__

#include <string>

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Timer.h"
MSVC_POP_WARNING();

#include "base/scoped_ptr.h"
#include "webkit/glue/resource_fetcher.h"
#include "webkit/glue/weberror_impl.h"

class ResourceFetcherWithTimeout;
class WebFrameImpl;
class WebRequest;
class WebView;

// Used for downloading alternate dns error pages. Once downloading is done
// (or fails), the webview delegate is notified.
class AltErrorPageResourceFetcher : public ResourceFetcher::Delegate {
 public:
  AltErrorPageResourceFetcher(WebView* web_view,
                              const WebErrorImpl& web_error,
                              WebFrameImpl* web_frame,
                              const GURL& url);
  ~AltErrorPageResourceFetcher();

  virtual void OnURLFetchComplete(const WebCore::ResourceResponse& response,
                                  const std::string& data);

 private:
  // References to our owners
  WebView* web_view_;
  WebErrorImpl web_error_;
  WebFrameImpl* web_frame_;
  scoped_ptr<WebRequest> failed_request_;

  // Does the actual fetching.
  scoped_ptr<ResourceFetcherWithTimeout> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AltErrorPageResourceFetcher);
};

#endif  // WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__

