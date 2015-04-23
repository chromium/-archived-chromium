// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the RenderProcessHosts, and dispatches them to URLRequests. It then
// fowards the messages from the URLRequests back to the correct process for
// handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_HANDLER_H_

#include <string>

#include "chrome/common/filter_policy.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace net {
class IOBuffer;
}

// Parameters for a resource response header.
struct ResourceResponseHead
    : webkit_glue::ResourceLoaderBridge::ResponseInfo {
  ResourceResponseHead() : filter_policy(FilterPolicy::DONT_FILTER) {}

  // The response status.
  URLRequestStatus status;

  // Specifies if the resource should be filtered before being displayed
  // (insecure resources can be filtered to keep the page secure).
  FilterPolicy::Type filter_policy;
};

// Parameters for a synchronous resource response.
struct SyncLoadResult : ResourceResponseHead {
  // The final URL after any redirects.
  GURL final_url;

  // The response data.
  std::string data;
};

// Simple wrapper that refcounts ResourceResponseHead.
struct ResourceResponse : public base::RefCounted<ResourceResponse> {
  ResourceResponseHead response_head;
};

// The resource dispatcher host uses this interface to push load events to the
// renderer, allowing for differences in the types of IPC messages generated.
// See the implementations of this interface defined below.
class ResourceHandler : public base::RefCounted<ResourceHandler> {
 public:
  virtual ~ResourceHandler() {}

  // Called as upload progress is made.
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) {
    return true;
  }

  // The request was redirected to a new URL.
  virtual bool OnRequestRedirected(int request_id, const GURL& url) = 0;

  // Response headers and meta data are available.
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) = 0;

  // Data will be read for the response.  Upon success, this method places the
  // size and address of the buffer where the data is to be written in its
  // out-params.  This call will be followed by either OnReadCompleted or
  // OnResponseCompleted, at which point the buffer may be recycled.
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) = 0;

  // Data (*bytes_read bytes) was written into the buffer provided by
  // OnWillRead. A return value of false cancels the request, true continues
  // reading data.
  virtual bool OnReadCompleted(int request_id, int* bytes_read) = 0;

  // The response is complete.  The final response status is given.
  // Returns false if the handler is deferring the call to a later time.
  virtual bool OnResponseCompleted(int request_id,
                                   const URLRequestStatus& status,
                                   const std::string& security_info) = 0;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_HANDLER_H_
