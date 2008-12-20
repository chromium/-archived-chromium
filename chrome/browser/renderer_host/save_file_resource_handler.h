// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAVE_FILE_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_SAVE_FILE_RESOURCE_HANDLER_H_

#include <string>

#include "chrome/browser/renderer_host/resource_handler.h"

class SaveFileManager;

// Forwards data to the save thread.
class SaveFileResourceHandler : public ResourceHandler {
 public:
  SaveFileResourceHandler(int render_process_host_id,
                          int render_view_id,
                          const std::string& url,
                          SaveFileManager* manager);

  // Saves the redirected URL to final_url_, we need to use the original
  // URL to match original request.
  bool OnRequestRedirected(int request_id, const GURL& url);

  // Sends the download creation information to the download thread.
  bool OnResponseStarted(int request_id, ResourceResponse* response);

  // Creates a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size);

  // Passes the buffer to the download file writer.
  bool OnReadCompleted(int request_id, int* bytes_read);

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status);

  // If the content-length header is not present (or contains something other
  // than numbers), StringToInt64 returns 0, which indicates 'unknown size' and
  // is handled correctly by the SaveManager.
  void set_content_length(const std::string& content_length) {
    content_length_ = StringToInt64(content_length);
  }

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

 private:
  int save_id_;
  int render_process_id_;
  int render_view_id_;
  char* read_buffer_;
  std::string content_disposition_;
  std::wstring url_;
  std::wstring final_url_;
  int64 content_length_;
  SaveFileManager* save_manager_;

  static const int kReadBufSize = 32768;  // bytes

  DISALLOW_COPY_AND_ASSIGN(SaveFileResourceHandler);
};
#endif  // CHROME_BROWSER_RENDERER_HOST_SAVE_FILE_RESOURCE_HANDLER_H_
