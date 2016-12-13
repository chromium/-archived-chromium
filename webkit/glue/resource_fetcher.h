// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around ResourceHandle and ResourceHandleClient that simplifies
// the download of an HTTP object.  The interface is modeled after URLFetcher
// in the /chrome/browser.
//
// ResourceFetcher::Delegate::OnURLFetchComplete will be called async after
// the ResourceFetcher object is created.

#ifndef WEBKIT_GLUE_RESOURCE_FETCHER_H_
#define WEBKIT_GLUE_RESOURCE_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "webkit/api/public/WebURLLoaderClient.h"
#include "webkit/api/public/WebURLResponse.h"

class GURL;
class WebFrame;

namespace WebKit {
class WebURLLoader;
class WebURLRequest;
struct WebURLError;
}

namespace webkit_glue {

class ResourceFetcher : public WebKit::WebURLLoaderClient {
 public:
  // This will be called when the URL has been fetched, successfully or not.
  // If there is a failure, response and data will both be empty.  |response|
  // and |data| are both valid until the URLFetcher instance is destroyed.
  typedef Callback2<const WebKit::WebURLResponse&,
                    const std::string&>::Type Callback;

  // We need a frame to make requests.
  ResourceFetcher(const GURL& url, WebFrame* frame, Callback* d);
  ~ResourceFetcher();

  // Stop the request and don't call the callback.
  void Cancel();

  bool completed() const { return completed_; }

 protected:
  // WebURLLoaderClient methods:
  virtual void willSendRequest(
      WebKit::WebURLLoader* loader, WebKit::WebURLRequest& new_request,
      const WebKit::WebURLResponse& redirect_response);
  virtual void didSendData(
      WebKit::WebURLLoader* loader, unsigned long long bytes_sent,
      unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(
      WebKit::WebURLLoader* loader, const WebKit::WebURLResponse& response);
  virtual void didReceiveData(
      WebKit::WebURLLoader* loader, const char* data, int data_length,
      long long total_data_length);
  virtual void didFinishLoading(WebKit::WebURLLoader* loader);
  virtual void didFail(
      WebKit::WebURLLoader* loader, const WebKit::WebURLError& error);

  scoped_ptr<WebKit::WebURLLoader> loader_;

  // URL we're fetching
  GURL url_;

  // Callback when we're done
  scoped_ptr<Callback> callback_;

  // A copy of the original resource response
  WebKit::WebURLResponse response_;

  // Set to true once the request is compelte.
  bool completed_;

 private:
  // Start the actual download.
  void Start(WebFrame* frame);

  // Buffer to hold the content from the server.
  std::string data_;
};

/////////////////////////////////////////////////////////////////////////////
// A resource fetcher with a timeout
class ResourceFetcherWithTimeout : public ResourceFetcher {
 public:
  ResourceFetcherWithTimeout(const GURL& url, WebFrame* frame,
                             int timeout_secs, Callback* c);
  virtual ~ResourceFetcherWithTimeout() {}

 private:
  // Callback for timer that limits how long we wait for the alternate error
  // page server.  If this timer fires and the request hasn't completed, we
  // kill the request.
  void TimeoutFired();

  // Limit how long we wait for the alternate error page server.
  base::OneShotTimer<ResourceFetcherWithTimeout> timeout_timer_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_RESOURCE_FETCHER_H_
