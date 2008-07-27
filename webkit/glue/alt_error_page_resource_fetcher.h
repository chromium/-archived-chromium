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

#ifndef WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__
#define WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__

#include <string>

#pragma warning(push, 0)
#include "Timer.h"
#pragma warning(pop)

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/resource_fetcher.h"
#include "webkit/glue/weberror_impl.h"
#include "webkit/glue/weburlrequest.h"

class WebCore::ResourceResponse;
class WebFrameImpl;
class WebView;

// Used for downloading alternate dns error pages. Once downloading is done
// (or fails), the webview delegate is notified.
class AltErrorPageResourceFetcher : public ResourceFetcher::Delegate {
 public:
  AltErrorPageResourceFetcher(WebView* web_view,
                              const WebErrorImpl& web_error,
                              WebFrameImpl* web_frame,
                              const GURL& url);

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

  DISALLOW_EVIL_CONSTRUCTORS(AltErrorPageResourceFetcher);
};

#endif  // WEBKIT_GLUE_ALT_ERROR_PAGE_RESOURCE_FETCHER_H__
