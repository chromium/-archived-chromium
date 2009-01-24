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

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

// Simple wrapper that refcounts ViewMsg_Resource_ResponseHead.
struct ResourceResponse : public base::RefCounted<ResourceResponse> {
  ViewMsg_Resource_ResponseHead response_head;
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
                          char** buf,
                          int* buf_size,
                          int min_size) = 0;

  // Data (*bytes_read bytes) was written into the buffer provided by
  // OnWillRead. A return value of false cancels the request, true continues
  // reading data.
  virtual bool OnReadCompleted(int request_id, int* bytes_read) = 0;

  // The response is complete.  The final response status is given.
  // Returns false if the handler is deferring the call to a later time.
  virtual bool OnResponseCompleted(int request_id,
                                   const URLRequestStatus& status) = 0;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_HANDLER_H_
