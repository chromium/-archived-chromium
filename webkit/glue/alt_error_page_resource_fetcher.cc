// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ResourceResponse.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/alt_error_page_resource_fetcher.h"

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview.h"

// Number of seconds to wait for the alternate error page server.  If it takes
// too long, just use the local error page.
static const double kDownloadTimeoutSec = 3.0;

AltErrorPageResourceFetcher::AltErrorPageResourceFetcher(
    WebView* web_view,
    const WebErrorImpl& web_error,
    WebFrameImpl* web_frame,
    const GURL& url)
    : web_view_(web_view),
      web_error_(web_error),
      web_frame_(web_frame) {
  failed_request_.reset(web_frame_->GetProvisionalDataSource()->
      GetRequest().Clone());
  fetcher_.reset(new ResourceFetcherWithTimeout(url, web_frame->frame(),
                                                kDownloadTimeoutSec, this));
}

AltErrorPageResourceFetcher::~AltErrorPageResourceFetcher() {
}

void AltErrorPageResourceFetcher::OnURLFetchComplete(
    const WebCore::ResourceResponse& response,
    const std::string& data) {
  WebViewDelegate* delegate = web_view_->GetDelegate();
  if (!delegate)
    return;

  if (response.httpStatusCode() == 200) {
    // We successfully got a response from the alternate error page server, so
    // load it.
    delegate->LoadNavigationErrorPage(web_frame_, failed_request_.get(),
                                      web_error_, data, true);
  } else {
    delegate->LoadNavigationErrorPage(web_frame_, failed_request_.get(),
                                      web_error_, std::string(), true);
  }
}
