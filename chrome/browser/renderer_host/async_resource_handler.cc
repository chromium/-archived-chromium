// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/async_resource_handler.h"

#include "base/process.h"
#include "base/shared_memory.h"
#include "chrome/common/render_messages.h"
#include "net/base/io_buffer.h"

SharedIOBuffer* AsyncResourceHandler::spare_read_buffer_;

// Our version of IOBuffer that uses shared memory.
class SharedIOBuffer : public net::IOBuffer {
 public:
  SharedIOBuffer(int buffer_size) : net::IOBuffer(), ok_(false) {
    if (shared_memory_.Create(std::wstring(), false, false, buffer_size) &&
        shared_memory_.Map(buffer_size)) {
      ok_ = true;
      data_ = reinterpret_cast<char*>(shared_memory_.memory());
    }
  }
  ~SharedIOBuffer() {
    data_ = NULL;
  }

  base::SharedMemory* shared_memory() { return &shared_memory_; }
  bool ok() { return ok_; }

 private:
  base::SharedMemory shared_memory_;
  bool ok_;
};

AsyncResourceHandler::AsyncResourceHandler(
    ResourceDispatcherHost::Receiver* receiver,
    int process_id,
    int routing_id,
    base::ProcessHandle process_handle,
    const GURL& url,
    ResourceDispatcherHost* resource_dispatcher_host)
    : receiver_(receiver),
      process_id_(process_id),
      routing_id_(routing_id),
      process_handle_(process_handle),
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

bool AsyncResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                      int* buf_size, int min_size) {
  DCHECK(min_size == -1);
  static const int kReadBufSize = 32768;
  if (spare_read_buffer_) {
    DCHECK(!read_buffer_);
    read_buffer_.swap(&spare_read_buffer_);
  } else {
    read_buffer_ = new SharedIOBuffer(kReadBufSize);
    if (!read_buffer_->ok())
      return false;
  }
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;
  return true;
}

bool AsyncResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (!*bytes_read)
    return true;
  DCHECK(read_buffer_.get());

  if (!rdh_->WillSendData(process_id_, request_id)) {
    // We should not send this data now, we have too many pending requests.
    return true;
  }

  base::SharedMemoryHandle handle;
  if (!read_buffer_->shared_memory()->GiveToProcess(process_handle_, &handle)) {
    // We wrongfully incremented the pending data count. Fake an ACK message
    // to fix this. We can't move this call above the WillSendData because
    // it's killing our read_buffer_, and we don't want that when we pause
    // the request.
    rdh_->DataReceivedACK(process_id_, request_id);
    // We just unmapped the memory.
    read_buffer_ = NULL;
    return false;
  }
  // We just unmapped the memory.
  read_buffer_ = NULL;

  receiver_->Send(new ViewMsg_Resource_DataReceived(
      routing_id_, request_id, handle, *bytes_read));

  return true;
}

bool AsyncResourceHandler::OnResponseCompleted(
    int request_id,
    const URLRequestStatus& status,
    const std::string& security_info) {
  receiver_->Send(new ViewMsg_Resource_RequestComplete(routing_id_,
                                                       request_id,
                                                       status,
                                                       security_info));

  // If we still have a read buffer, then see about caching it for later...
  if (spare_read_buffer_) {
    read_buffer_ = NULL;
  } else if (read_buffer_.get()) {
    read_buffer_.swap(&spare_read_buffer_);
  }
  return true;
}

// static
void AsyncResourceHandler::GlobalCleanup() {
  if (spare_read_buffer_) {
    spare_read_buffer_->Release();
    spare_read_buffer_ = NULL;
  }
}
