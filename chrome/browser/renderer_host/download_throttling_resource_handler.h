// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_THROTTLING_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_THROTTLING_RESOURCE_HANDLER_H_

#include <string>

#include "chrome/browser/renderer_host/resource_handler.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN)
// TODO(port): Remove ifdef when downloads are ported.
#include "chrome/browser/download/download_request_manager.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class DownloadResourceHandler;
class ResourceDispatcherHost;

// DownloadThrottlingResourceHandler is used to determine if a download should
// be allowed. When a DownloadThrottlingResourceHandler is created it pauses the
// download and asks the DownloadRequestManager if the download should be
// allowed. The DownloadRequestManager notifies us asynchronously as to whether
// the download is allowed or not. If the download is allowed the request is
// resumed, a DownloadResourceHandler is created and all EventHandler methods
// are delegated to it. If the download is not allowed the request is canceled.

class DownloadThrottlingResourceHandler
    : public ResourceHandler,
      public DownloadRequestManager::Callback {
 public:
  DownloadThrottlingResourceHandler(ResourceDispatcherHost* host,
                                    URLRequest* request,
                                    const GURL& url,
                                    int render_process_host_id,
                                    int render_view_id,
                                    int request_id,
                                    bool in_complete);
  virtual ~DownloadThrottlingResourceHandler();

  // ResourceHanlder implementation:
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size);
  virtual bool OnRequestRedirected(int request_id, const GURL& url);
  virtual bool OnResponseStarted(int request_id, ResourceResponse* response);
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                          int min_size);
  virtual bool OnReadCompleted(int request_id, int* bytes_read);
  virtual bool OnResponseCompleted(int request_id,
                                   const URLRequestStatus& status);

  // DownloadRequestManager::Callback implementation:
  void CancelDownload();
  void ContinueDownload();

 private:
  void CopyTmpBufferToDownloadHandler();

  ResourceDispatcherHost* host_;
  URLRequest* request_;
  GURL url_;
  int render_process_host_id_;
  int render_view_id_;
  int request_id_;

  // Handles the actual download. This is only created if the download is
  // allowed to continue.
  scoped_refptr<DownloadResourceHandler> download_handler_;

  // Response supplied to OnResponseStarted. Only non-null if OnResponseStarted
  // is invoked.
  scoped_refptr<ResourceResponse> response_;

  // If we're created by way of BufferedEventHandler we'll get one request for
  // a buffer. This is that buffer.
  scoped_refptr<net::IOBuffer> tmp_buffer_;
  int tmp_buffer_length_;

  // If true the next call to OnReadCompleted is ignored. This is used if we're
  // paused during a call to OnReadCompleted. Pausing during OnReadCompleted
  // results in two calls to OnReadCompleted for the same data. This make sure
  // we ignore one of them.
  bool ignore_on_read_complete_;

  DISALLOW_COPY_AND_ASSIGN(DownloadThrottlingResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_THROTTLING_RESOURCE_HANDLER_H_
