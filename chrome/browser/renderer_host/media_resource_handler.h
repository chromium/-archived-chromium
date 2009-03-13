// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_MEDIA_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_MEDIA_RESOURCE_HANDLER_H_

#include "base/process.h"
#include "base/platform_file.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_handler.h"

// Used to complete a media resource request in response to resource load events
// from the resource dispatcher host. This handler only works asynchronously and
// tries to work with file for response data if possible. If a response data
// file is not available, it redirects calls to underlying handler.
class MediaResourceHandler : public ResourceHandler {
 public:
  MediaResourceHandler(ResourceHandler* resource_handler,
                       ResourceDispatcherHost::Receiver* receiver,
                       int render_process_host_id,
                       int routing_id,
                       base::ProcessHandle render_process,
                       ResourceDispatcherHost* resource_dispatcher_host);

  // ResourceHandler implementation:
  bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  bool OnRequestRedirected(int request_id, const GURL& new_url);
  bool OnResponseStarted(int request_id, ResourceResponse* response);
  bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                  int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);
  bool OnResponseCompleted(int request_id, const URLRequestStatus& status,
                           const std::string& security_info);

 private:
  ResourceDispatcherHost::Receiver* receiver_;
  int render_process_host_id_;
  int routing_id_;
  base::ProcessHandle render_process_;
  scoped_refptr<ResourceHandler> handler_;
  ResourceDispatcherHost* rdh_;
  bool has_file_handle_;
  int64 position_;
  int64 size_;

  DISALLOW_COPY_AND_ASSIGN(MediaResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_MEDIA_RESOURCE_HANDLER_H_
