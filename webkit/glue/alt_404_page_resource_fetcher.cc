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
#include "webkit/glue/webframeloaderclient_impl.h"

using WebCore::DocumentLoader;

// Number of seconds to wait for the alternate 404 page server.  If it takes
// too long, just show the original 404 page.
static const double kDownloadTimeoutSec = 3.0;

Alt404PageResourceFetcher::Alt404PageResourceFetcher(
    WebFrameLoaderClient* webframeloaderclient,
    WebCore::Frame* frame,
    DocumentLoader* doc_loader,
    const GURL& url)
    : webframeloaderclient_(webframeloaderclient),
      doc_loader_(doc_loader) {

  fetcher_.reset(new ResourceFetcherWithTimeout(url, frame,
                                                kDownloadTimeoutSec, this));
}

void Alt404PageResourceFetcher::OnURLFetchComplete(
    const WebCore::ResourceResponse& response,
    const std::string& data) {
  if (response.httpStatusCode() == 200) {
    // Only show server response if we got a 200.
    webframeloaderclient_->Alt404PageFinished(doc_loader_.get(), data);
  } else {
    webframeloaderclient_->Alt404PageFinished(doc_loader_.get(), std::string());
  }
  doc_loader_ = NULL;
}
