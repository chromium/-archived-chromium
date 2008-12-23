// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/sync_resource_handler.h"

SyncResourceHandler::SyncResourceHandler(
    ResourceDispatcherHost::Receiver* receiver,
    const GURL& url,
    IPC::Message* result_message)
    : receiver_(receiver),
      result_message_(result_message) {
  result_.final_url = url;
  result_.filter_policy = FilterPolicy::DONT_FILTER;
}

bool SyncResourceHandler::OnRequestRedirected(int request_id,
                                              const GURL& new_url) {
  result_.final_url = new_url;
  return true;
}

bool SyncResourceHandler::OnResponseStarted(int request_id,
                                            ResourceResponse* response) {
  // We don't care about copying the status here.
  result_.headers = response->response_head.headers;
  result_.mime_type = response->response_head.mime_type;
  result_.charset = response->response_head.charset;
  return true;
}

bool SyncResourceHandler::OnWillRead(int request_id,
                                     char** buf, int* buf_size, int min_size) {
  DCHECK(min_size == -1);
  *buf = read_buffer_;
  *buf_size = kReadBufSize;
  return true;
}

bool SyncResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (!*bytes_read)
    return true;
  result_.data.append(read_buffer_, *bytes_read);
  return true;
}

bool SyncResourceHandler::OnResponseCompleted(int request_id,
                                              const URLRequestStatus& status) {
  result_.status = status;

  ViewHostMsg_SyncLoad::WriteReplyParams(result_message_, result_);
  receiver_->Send(result_message_);
  return true;
}
