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
//
// A wrapper around ResourceHandle and ResourceHandleClient that simplifies
// the download of an HTTP object.  The interface is modeled after URLFetcher
// in the /chrome/browser.
//
// ResourceFetcher::Delegate::OnURLFetchComplete will be called async after
// the ResourceFetcher object is created.

#ifndef WEBKIT_GLUE_RESOURCE_FETCHER_H__
#define WEBKIT_GLUE_RESOURCE_FETCHER_H__

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"

#pragma warning(push, 0)
#include "Frame.h"
#include "Timer.h"
#include "ResourceHandleClient.h"
#include "ResourceResponse.h"
#pragma warning(pop)

class GURL;
class WebCore::ResourceHandle;

class ResourceFetcher : public WebCore::ResourceHandleClient {
 public:
  class Delegate {
   public:
    // This will be called when the URL has been fetched, successfully or not.
    // If there is a failure, response and data will both be empty.
    // |response| and |data| are both valid until the URLFetcher instance is
    // destroyed.
    virtual void OnURLFetchComplete(const WebCore::ResourceResponse& response,
                                    const std::string& data) = 0;
  };

  // We need a frame and frame loader to make requests.
  ResourceFetcher(const GURL& url, WebCore::Frame* frame, Delegate* d);
  ~ResourceFetcher();

  // Stop the request and don't call the callback.
  void Cancel();

  bool completed() { return completed_; }

  // ResourceHandleClient methods
  virtual void didReceiveResponse(WebCore::ResourceHandle* resource_handle,
                                  const WebCore::ResourceResponse& response);

  virtual void didReceiveData(WebCore::ResourceHandle* resource_handle, 
                              const char* data, int length, int total_length);

  virtual void didFinishLoading(WebCore::ResourceHandle* resource_handle);

  virtual void didFail(WebCore::ResourceHandle* resource_handle, 
                       const WebCore::ResourceError& error);

 protected:
  // The parent ResourceHandle
  RefPtr<WebCore::ResourceHandle> loader_;

  // URL we're fetching
  GURL url_;

  // Callback when we're done
  Delegate* delegate_;

  // A copy of the original resource response
  WebCore::ResourceResponse response_;

  // Set to true once the request is compelte.
  bool completed_;

 private:
  // If we fail to start the request, we still want to finish async.
  typedef WebCore::Timer<ResourceFetcher> StartFailedTimer;

  // Start the actual download.
  void Start(WebCore::Frame* frame);

  // Callback function if Start fails.
  void StartFailed(StartFailedTimer* timer);

  // Timer for calling StartFailed async.
  scoped_ptr<StartFailedTimer> start_failed_timer_;

  // Buffer to hold the content from the server.
  std::string data_;
};

/////////////////////////////////////////////////////////////////////////////
// A resource fetcher with a timeout
class ResourceFetcherWithTimeout : public ResourceFetcher {
 public:
  ResourceFetcherWithTimeout(const GURL& url, WebCore::Frame* frame, double
                             timeout_secs, Delegate* d);
  virtual ~ResourceFetcherWithTimeout() {}

 private:
  typedef WebCore::Timer<ResourceFetcherWithTimeout> FetchTimer;

  // Callback for timer that limits how long we wait for the alternate error
  // page server.  If this timer fires and the request hasn't completed, we
  // kill the request.
  void TimeoutFired(FetchTimer* timer);

  // Limit how long we wait for the alternate error page server.
  scoped_ptr<FetchTimer> timeout_timer_;
};

#endif  // WEBKIT_GLUE_RESOURCE_FETCHER_H__
