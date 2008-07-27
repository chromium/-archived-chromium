// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#pragma warning(push, 0)
#include "ResourceResponse.h"
#pragma warning(pop)
#undef LOG

#include "webkit/glue/alt_error_page_resource_fetcher.h"

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
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
