// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_

#include <string>

#include "chrome/browser/renderer_host/resource_handler.h"

class ResourceDispatcherHost;

// Used to buffer a request until enough data has been received.
class BufferedResourceHandler : public ResourceHandler {
 public:
  BufferedResourceHandler(ResourceHandler* handler,
                          ResourceDispatcherHost* host,
                          URLRequest* request);

  // ResourceHandler implementation:
  bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  bool OnRequestRedirected(int request_id, const GURL& new_url);
  bool OnResponseStarted(int request_id, ResourceResponse* response);
  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);
  bool OnResponseCompleted(int request_id, const URLRequestStatus& status);

 private:
  // Returns true if we should delay OnResponseStarted forwarding.
  bool DelayResponse();

  // Returns true if there will be a need to parse the DocType of the document
  // to determine the right way to handle it.
  bool ShouldBuffer(const GURL& url, const std::string& mime_type);

  // Returns true if there is enough information to process the DocType.
  bool DidBufferEnough(int bytes_read);

  // Returns true if we have to keep buffering data.
  bool KeepBuffering(int bytes_read);

  // Sends a pending OnResponseStarted notification. |in_complete| is true if
  // this is invoked from |OnResponseCompleted|.
  bool CompleteResponseStarted(int request_id, bool in_complete);

  scoped_refptr<ResourceHandler> real_handler_;
  scoped_refptr<ResourceResponse> response_;
  ResourceDispatcherHost* host_;
  URLRequest* request_;
  char* read_buffer_;
  int read_buffer_size_;
  int bytes_read_;
  bool sniff_content_;
  bool should_buffer_;
  bool buffering_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BUFFERED_RESOURCE_HANDLER_H_
