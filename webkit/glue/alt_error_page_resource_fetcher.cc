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

#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview.h"

using WebKit::WebURLError;
using WebKit::WebURLResponse;

namespace webkit_glue {

// Number of seconds to wait for the alternate error page server.  If it takes
// too long, just use the local error page.
static const int kDownloadTimeoutSec = 3;

AltErrorPageResourceFetcher::AltErrorPageResourceFetcher(
    WebView* web_view,
    WebFrame* web_frame,
    const WebURLError& web_error,
    const GURL& url)
    : web_view_(web_view),
      web_error_(web_error),
      web_frame_(web_frame) {
  failed_request_ = web_frame_->GetProvisionalDataSource()->request();
  fetcher_.reset(new ResourceFetcherWithTimeout(
      url, web_frame, kDownloadTimeoutSec,
      NewCallback(this, &AltErrorPageResourceFetcher::OnURLFetchComplete)));
}

AltErrorPageResourceFetcher::~AltErrorPageResourceFetcher() {
}

void AltErrorPageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  WebViewDelegate* delegate = web_view_->GetDelegate();
  if (!delegate)
    return;

  // A null response indicates a network error.
  if (!response.isNull() && response.httpStatusCode() == 200) {
    // We successfully got a response from the alternate error page server, so
    // load it.
    delegate->LoadNavigationErrorPage(web_frame_, failed_request_,
                                      web_error_, data, true);
  } else {
    delegate->LoadNavigationErrorPage(web_frame_, failed_request_,
                                      web_error_, std::string(), true);
  }
}

}  // namespace webkit_glue
