// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "DocumentLoader.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/alt_404_page_resource_fetcher.h"

#include "googleurl/src/gurl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webframeloaderclient_impl.h"

using WebCore::DocumentLoader;
using WebKit::WebURLResponse;

namespace webkit_glue {

// Number of seconds to wait for the alternate 404 page server.  If it takes
// too long, just show the original 404 page.
static const int kDownloadTimeoutSec = 3;

Alt404PageResourceFetcher::Alt404PageResourceFetcher(
    WebFrameLoaderClient* webframeloaderclient,
    WebCore::Frame* frame,
    DocumentLoader* doc_loader,
    const GURL& url)
    : webframeloaderclient_(webframeloaderclient),
      doc_loader_(doc_loader) {

  fetcher_.reset(new ResourceFetcherWithTimeout(
      url, WebFrameImpl::FromFrame(frame), kDownloadTimeoutSec,
      NewCallback(this, &Alt404PageResourceFetcher::OnURLFetchComplete)));
}

void Alt404PageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  // A null response indicates a network error.
  if (!response.isNull() && response.httpStatusCode() == 200) {
    // Only show server response if we got a 200.
    webframeloaderclient_->Alt404PageFinished(doc_loader_.get(), data);
  } else {
    webframeloaderclient_->Alt404PageFinished(doc_loader_.get(), std::string());
  }
  doc_loader_ = NULL;
}

}  // namespace webkit_glue
