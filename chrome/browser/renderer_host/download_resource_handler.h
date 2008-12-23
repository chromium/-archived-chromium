// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_RESOURCE_HANDLER_H_

#include <string>

#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_handler.h"

struct DownloadBuffer;

// Forwards data to the download thread.
class DownloadResourceHandler : public ResourceHandler {
 public:
  DownloadResourceHandler(ResourceDispatcherHost* rdh,
                          int render_process_host_id,
                          int render_view_id,
                          int request_id,
                          const std::string& url,
                          DownloadFileManager* manager,
                          URLRequest* request,
                          bool save_as);

  // Not needed, as this event handler ought to be the final resource.
  bool OnRequestRedirected(int request_id, const GURL& url);

  // Send the download creation information to the download thread.
  bool OnResponseStarted(int request_id, ResourceResponse* response);

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size);

  bool OnReadCompleted(int request_id, int* bytes_read);

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status);

  // If the content-length header is not present (or contains something other
  // than numbers), the incoming content_length is -1 (unknown size).
  // Set the content length to 0 to indicate unknown size to DownloadManager.
  void set_content_length(const int64& content_length);

  void set_content_disposition(const std::string& content_disposition);

  void CheckWriteProgress();

 private:
  void StartPauseTimer();

  int download_id_;
  ResourceDispatcherHost::GlobalRequestID global_id_;
  int render_view_id_;
  char* read_buffer_;
  std::string content_disposition_;
  std::wstring url_;
  int64 content_length_;
  DownloadFileManager* download_manager_;
  URLRequest* request_;
  bool save_as_;  // Request was initiated via "Save As" by the user.
  DownloadBuffer* buffer_;
  ResourceDispatcherHost* rdh_;
  bool is_paused_;
  base::OneShotTimer<DownloadResourceHandler> pause_timer_;

  static const int kReadBufSize = 32768;  // bytes
  static const int kLoadsToWrite = 100;  // number of data buffers queued
  static const int kThrottleTimeMs = 200;  // milliseconds

  DISALLOW_COPY_AND_ASSIGN(DownloadResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_RESOURCE_HANDLER_H_
