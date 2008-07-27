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
#include "webkit/glue/resource_fetcher.h"

#pragma warning(push, 0)
#include "FrameLoader.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#pragma warning(pop)

#undef LOG
#include "base/logging.h"
#include "webkit/glue/glue_util.h"
#include "net/url_request/url_request_status.h"

using WebCore::ResourceError;
using WebCore::ResourceHandle;
using WebCore::ResourceResponse;

ResourceFetcher::ResourceFetcher(const GURL& url, WebCore::Frame* frame,
                                 Delegate* d)
    : url_(url), delegate_(d), completed_(false) {
  // Can't do anything without a frame.  However, delegate can be NULL (so we
  // can do a http request and ignore the results).
  DCHECK(frame);
  Start(frame);
}

ResourceFetcher::~ResourceFetcher() {
  if (!completed_)
    loader_->cancel();
  loader_ = NULL;
}

void ResourceFetcher::Cancel() {
  if (!completed_) {
    loader_->cancel();
    completed_ = true;
  }
}

void ResourceFetcher::Start(WebCore::Frame* frame) {
  WebCore::FrameLoader* frame_loader = frame->loader();
  if (!frame_loader) {
    // We put this on a 0 timer so the callback happens async (consistent with
    // regular fetches).
    start_failed_timer_.reset(new StartFailedTimer(this,
          &ResourceFetcher::StartFailed));
    start_failed_timer_->startOneShot(0);
    return;
  }

  WebCore::ResourceRequest request(webkit_glue::GURLToKURL(url_));
  request.setFrame(frame);

  loader_ = ResourceHandle::create(request, this, NULL, false, false);
}

void ResourceFetcher::StartFailed(StartFailedTimer* timer) {
  didFail(NULL, ResourceError());
}

/////////////////////////////////////////////////////////////////////////////
// ResourceHandleClient methods
void ResourceFetcher::didReceiveResponse(ResourceHandle* resource_handle,
                                         const ResourceResponse& response) {
  ASSERT(!completed_);
  // It's safe to use the ResourceResponse copy constructor
  // (xmlhttprequest.cpp uses it).
  response_ = response;
}

void ResourceFetcher::didReceiveData(ResourceHandle* resource_handle,
                                     const char* data, int length,
                                     int total_length) {
  ASSERT(!completed_);
  if (length <= 0)
    return;

  data_.append(data, length);
}

void ResourceFetcher::didFinishLoading(ResourceHandle* resource_handle) {
  ASSERT(!completed_);
  completed_ = true;

  if (delegate_)
    delegate_->OnURLFetchComplete(response_, data_);
}

void ResourceFetcher::didFail(ResourceHandle* resource_handle, 
                              const ResourceError& error) {
  ASSERT(!completed_);
  completed_ = true;
  
  // Go ahead and tell our delegate that we're done.  Send an empty
  // ResourceResponse and string.
  if (delegate_)
    delegate_->OnURLFetchComplete(ResourceResponse(), std::string());
}

/////////////////////////////////////////////////////////////////////////////
// A resource fetcher with a timeout

ResourceFetcherWithTimeout::ResourceFetcherWithTimeout(
    const GURL& url, WebCore::Frame* frame, double timeout_secs, Delegate* d)
    : ResourceFetcher(url, frame, d) {
  timeout_timer_.reset(new FetchTimer(this,
      &ResourceFetcherWithTimeout::TimeoutFired));
  timeout_timer_->startOneShot(timeout_secs);
}

void ResourceFetcherWithTimeout::TimeoutFired(FetchTimer* timer) {
  if (!completed_) {
    loader_->cancel();
    didFail(NULL, ResourceError());
  }
}
