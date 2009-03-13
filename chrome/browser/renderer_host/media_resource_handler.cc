// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/media_resource_handler.h"

#include "base/process.h"
#include "chrome/common/render_messages.h"
#include "net/base/load_flags.h"

MediaResourceHandler::MediaResourceHandler(
    ResourceHandler* resource_handler,
    ResourceDispatcherHost::Receiver* receiver,
    int render_process_host_id,
    int routing_id,
    base::ProcessHandle render_process,
    ResourceDispatcherHost* resource_dispatcher_host)
    : receiver_(receiver),
      render_process_host_id_(render_process_host_id),
      routing_id_(routing_id),
      render_process_(render_process),
      handler_(resource_handler),
      rdh_(resource_dispatcher_host),
      has_file_handle_(false),
      position_(0),
      size_(-1) {
}

bool MediaResourceHandler::OnUploadProgress(int request_id,
                                            uint64 position,
                                            uint64 size) {
  return handler_->OnUploadProgress(request_id, position, size);
}

bool MediaResourceHandler::OnRequestRedirected(int request_id,
                                               const GURL& new_url) {
  return handler_->OnRequestRedirected(request_id, new_url);
}

bool MediaResourceHandler::OnResponseStarted(int request_id,
                                             ResourceResponse* response) {
#if defined(OS_POSIX)
  if (response->response_head.response_data_file.fd !=
      base::kInvalidPlatformFileValue) {
    // On POSIX, we will just set auto_close to true, and the IPC infrastructure
    // will send this file handle through and close it automatically.
    response->response_head.response_data_file.auto_close = true;
    has_file_handle_ = true;
  }
#elif defined(OS_WIN)
  if (response->response_head.response_data_file !=
      base::kInvalidPlatformFileValue) {
    // On Windows, we duplicate the file handle for the renderer process and
    // close the original manually.
    base::PlatformFile foreign_handle;
    if (DuplicateHandle(GetCurrentProcess(),
                        response->response_head.response_data_file,
                        render_process_,
                        &foreign_handle,
                        FILE_READ_DATA,  // Only allow read access to data.
                        false,           // Foreign handle is not inheritable.
                        // Close the file handle after duplication.
                        DUPLICATE_CLOSE_SOURCE)){
      response->response_head.response_data_file = foreign_handle;
      has_file_handle_ = true;
    } else {
      has_file_handle_ = false;
    }
  }
#endif
  size_ = response->response_head.content_length;
  return handler_->OnResponseStarted(request_id, response);
}

bool MediaResourceHandler::OnWillRead(int request_id,
                                      net::IOBuffer** buf, int* buf_size,
                                      int min_size) {
  return handler_->OnWillRead(request_id, buf, buf_size, min_size);
}

bool MediaResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (has_file_handle_) {
    // If we have received a file handle before we will be sending a progress
    // update for download.
    // TODO(hclam): rate limit this message so we won't be sending too much to
    // the renderer process.
    receiver_->Send(
        new ViewMsg_Resource_DownloadProgress(routing_id_, request_id,
                                              position_, size_));
    position_ += *bytes_read;
  }
  return handler_->OnReadCompleted(request_id, bytes_read);
}

bool MediaResourceHandler::OnResponseCompleted(int request_id,
    const URLRequestStatus& status, const std::string& security_info) {
  return handler_->OnResponseCompleted(request_id, status, security_info);
}
