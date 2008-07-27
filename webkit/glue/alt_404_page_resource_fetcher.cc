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
#include "DocumentLoader.h"
#pragma warning(pop)
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
