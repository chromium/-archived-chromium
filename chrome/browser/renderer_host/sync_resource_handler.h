// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_

#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "net/base/io_buffer.h"

// Used to complete a synchronous resource request in response to resource load
// events from the resource dispatcher host.
class SyncResourceHandler : public ResourceHandler {
 public:
  SyncResourceHandler(ResourceDispatcherHost::Receiver* receiver,
                      const GURL& url,
                      IPC::Message* result_message);

  bool OnRequestRedirected(int request_id, const GURL& new_url);
  bool OnResponseStarted(int request_id, ResourceResponse* response);
  bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                  int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);
  bool OnResponseCompleted(int request_id, const URLRequestStatus& status);

 private:
  enum { kReadBufSize = 3840 };
  scoped_refptr<net::IOBuffer> read_buffer_;

  ViewHostMsg_SyncLoad_Result result_;
  ResourceDispatcherHost::Receiver* receiver_;
  IPC::Message* result_message_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
