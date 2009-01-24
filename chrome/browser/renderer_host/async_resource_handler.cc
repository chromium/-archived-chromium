// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/async_resource_handler.h"

#include "base/process.h"

base::SharedMemory* AsyncResourceHandler::spare_read_buffer_;

AsyncResourceHandler::AsyncResourceHandler(
    ResourceDispatcherHost::Receiver* receiver,
    int render_process_host_id,
    int routing_id,
    base::ProcessHandle render_process,
    const GURL& url,
    ResourceDispatcherHost* resource_dispatcher_host)
    : receiver_(receiver),
      render_process_host_id_(render_process_host_id),
      routing_id_(routing_id),
      render_process_(render_process),
      rdh_(resource_dispatcher_host) {
}

bool AsyncResourceHandler::OnUploadProgress(int request_id,
                                            uint64 position,
                                            uint64 size) {
  return receiver_->Send(new ViewMsg_Resource_UploadProgress(routing_id_,
                                                             request_id,
                                                             position, size));
}

bool AsyncResourceHandler::OnRequestRedirected(int request_id,
                                               const GURL& new_url) {
  return receiver_->Send(new ViewMsg_Resource_ReceivedRedirect(routing_id_,
                                                               request_id,
                                                               new_url));
}

bool AsyncResourceHandler::OnResponseStarted(int request_id,
                                             ResourceResponse* response) {
  receiver_->Send(new ViewMsg_Resource_ReceivedResponse(
      routing_id_, request_id, response->response_head));
  return true;
}

bool AsyncResourceHandler::OnWillRead(int request_id,
                                      char** buf, int* buf_size,
                                      int min_size) {
  DCHECK(min_size == -1);
  static const int kReadBufSize = 32768;
  if (spare_read_buffer_) {
    read_buffer_.reset(spare_read_buffer_);
    spare_read_buffer_ = NULL;
  } else {
    read_buffer_.reset(new base::SharedMemory);
    if (!read_buffer_->Create(std::wstring(), false, false, kReadBufSize))
      return false;
    if (!read_buffer_->Map(kReadBufSize))
      return false;
  }
  *buf = static_cast<char*>(read_buffer_->memory());
  *buf_size = kReadBufSize;
  return true;
}

bool AsyncResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (!*bytes_read)
    return true;
  DCHECK(read_buffer_.get());

  if (!rdh_->WillSendData(render_process_host_id_, request_id)) {
    // We should not send this data now, we have too many pending requests.
    return true;
  }

  base::SharedMemoryHandle handle;
  if (!read_buffer_->GiveToProcess(render_process_, &handle)) {
    // We wrongfully incremented the pending data count. Fake an ACK message
    // to fix this. We can't move this call above the WillSendData because
    // it's killing our read_buffer_, and we don't want that when we pause
    // the request.
    rdh_->OnDataReceivedACK(render_process_host_id_, request_id);
    return false;
  }

  receiver_->Send(new ViewMsg_Resource_DataReceived(
      routing_id_, request_id, handle, *bytes_read));

  return true;
}

bool AsyncResourceHandler::OnResponseCompleted(int request_id,
                                               const URLRequestStatus& status) {
  receiver_->Send(new ViewMsg_Resource_RequestComplete(routing_id_,
                                                       request_id, status));

  // If we still have a read buffer, then see about caching it for later...
  if (spare_read_buffer_) {
    read_buffer_.reset();
  } else if (read_buffer_.get() && read_buffer_->memory()) {
    spare_read_buffer_ = read_buffer_.release();
  }
  return true;
}

// static
void AsyncResourceHandler::GlobalCleanup() {
  delete spare_read_buffer_;
  spare_read_buffer_ = NULL;
}
