// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_ASYNC_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_ASYNC_RESOURCE_HANDLER_H_

#include "base/process.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_handler.h"

namespace base {
class SharedMemory;
}

// Used to complete an asynchronous resource request in response to resource
// load events from the resource dispatcher host.
class AsyncResourceHandler : public ResourceHandler {
 public:
  AsyncResourceHandler(ResourceDispatcherHost::Receiver* receiver,
                       int render_process_host_id,
                       int routing_id,
                       base::ProcessHandle render_process,
                       const GURL& url,
                       ResourceDispatcherHost* resource_dispatcher_host);

  // ResourceHandler implementation:
  bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  bool OnRequestRedirected(int request_id, const GURL& new_url);
  bool OnResponseStarted(int request_id, ResourceResponse* response);
  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);
  bool OnResponseCompleted(int request_id, const URLRequestStatus& status);

  static void GlobalCleanup();

 private:
  // When reading, we don't know if we are going to get EOF (0 bytes read), so
  // we typically have a buffer that we allocated but did not use.  We keep
  // this buffer around for the next read as a small optimization.
  static base::SharedMemory* spare_read_buffer_;

  scoped_ptr<base::SharedMemory> read_buffer_;
  ResourceDispatcherHost::Receiver* receiver_;
  int render_process_host_id_;
  int routing_id_;
  base::ProcessHandle render_process_;
  ResourceDispatcherHost* rdh_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_ASYNC_RESOURCE_HANDLER_H_
