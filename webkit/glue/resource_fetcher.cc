// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_fetcher.h"

#include "base/logging.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebKitClient.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebURLLoader.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/webframe.h"

using base::TimeDelta;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

ResourceFetcher::ResourceFetcher(const GURL& url, WebFrame* frame,
                                 Callback* c)
    : url_(url),
      callback_(c),
      completed_(false) {
  // Can't do anything without a frame.  However, delegate can be NULL (so we
  // can do a http request and ignore the results).
  DCHECK(frame);
  Start(frame);
}

ResourceFetcher::~ResourceFetcher() {
  if (!completed_ && loader_.get())
    loader_->cancel();
}

void ResourceFetcher::Cancel() {
  if (!completed_) {
    loader_->cancel();
    completed_ = true;
  }
}

void ResourceFetcher::Start(WebFrame* frame) {
  WebURLRequest request(url_);
  frame->DispatchWillSendRequest(&request);

  loader_.reset(WebKit::webKitClient()->createURLLoader());
  loader_->loadAsynchronously(request, this);
}

/////////////////////////////////////////////////////////////////////////////
// WebURLLoaderClient methods

void ResourceFetcher::willSendRequest(
    WebURLLoader* loader, WebURLRequest& new_request,
    const WebURLResponse& redirect_response) {
}

void ResourceFetcher::didSendData(
    WebURLLoader* loader, unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
}

void ResourceFetcher::didReceiveResponse(
    WebURLLoader* loader, const WebURLResponse& response) {
  DCHECK(!completed_);
  response_ = response;
}

void ResourceFetcher::didReceiveData(
    WebURLLoader* loader, const char* data, int data_length,
    long long total_data_length) {
  DCHECK(!completed_);
  DCHECK(data_length > 0);

  data_.append(data, data_length);
}

void ResourceFetcher::didFinishLoading(WebURLLoader* loader) {
  DCHECK(!completed_);
  completed_ = true;

  if (callback_)
    callback_->Run(response_, data_);
}

void ResourceFetcher::didFail(WebURLLoader* loader, const WebURLError& error) {
  DCHECK(!completed_);
  completed_ = true;

  // Go ahead and tell our delegate that we're done.
  if (callback_)
    callback_->Run(WebURLResponse(), std::string());
}

/////////////////////////////////////////////////////////////////////////////
// A resource fetcher with a timeout

ResourceFetcherWithTimeout::ResourceFetcherWithTimeout(
    const GURL& url, WebFrame* frame, int timeout_secs, Callback* c)
    : ResourceFetcher(url, frame, c) {
  timeout_timer_.Start(TimeDelta::FromSeconds(timeout_secs), this,
                       &ResourceFetcherWithTimeout::TimeoutFired);
}

void ResourceFetcherWithTimeout::TimeoutFired() {
  if (!completed_) {
    loader_->cancel();
    didFail(NULL, WebURLError());
  }
}

}  // namespace webkit_glue
