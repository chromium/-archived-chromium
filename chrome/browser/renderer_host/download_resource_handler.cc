// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/download_resource_handler.h"

#if defined(OS_POSIX)
// TODO(port): Remove the temporary scaffolding after porting the headers below.
#include "chrome/common/temp_scaffolding_stubs.h"
#elif defined(OS_WIN)
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/download/download_manager.h"
#endif

#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "net/base/io_buffer.h"

DownloadResourceHandler::DownloadResourceHandler(ResourceDispatcherHost* rdh,
                                                 int render_process_host_id,
                                                 int render_view_id,
                                                 int request_id,
                                                 const std::string& url,
                                                 DownloadFileManager* manager,
                                                 URLRequest* request,
                                                 bool save_as)
    : download_id_(-1),
      global_id_(ResourceDispatcherHost::GlobalRequestID(render_process_host_id,
                                                         request_id)),
      render_view_id_(render_view_id),
      url_(UTF8ToWide(url)),
      content_length_(0),
      download_manager_(manager),
      request_(request),
      save_as_(save_as),
      buffer_(new DownloadBuffer),
      rdh_(rdh),
      is_paused_(false) {
}

// Not needed, as this event handler ought to be the final resource.
bool DownloadResourceHandler::OnRequestRedirected(int request_id,
                                                  const GURL& url) {
  url_ = UTF8ToWide(url.spec());
  return true;
}

// Send the download creation information to the download thread.
bool DownloadResourceHandler::OnResponseStarted(int request_id,
                                                ResourceResponse* response) {
  std::string content_disposition;
  request_->GetResponseHeaderByName("content-disposition",
                                    &content_disposition);
  set_content_disposition(content_disposition);
  set_content_length(response->response_head.content_length);

  download_id_ = download_manager_->GetNextId();
  // |download_manager_| consumes (deletes):
  DownloadCreateInfo* info = new DownloadCreateInfo;
  info->url = url_;
  info->start_time = base::Time::Now();
  info->received_bytes = 0;
  info->total_bytes = content_length_;
  info->state = DownloadItem::IN_PROGRESS;
  info->download_id = download_id_;
  info->render_process_id = global_id_.render_process_host_id;
  info->render_view_id = render_view_id_;
  info->request_id = global_id_.request_id;
  info->content_disposition = content_disposition_;
  info->mime_type = response->response_head.mime_type;
  info->save_as = save_as_;
  info->is_dangerous = false;
  download_manager_->file_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(download_manager_,
                        &DownloadFileManager::StartDownload,
                        info));
  return true;
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
bool DownloadResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                         int* buf_size, int min_size) {
  DCHECK(buf && buf_size);
  if (!read_buffer_) {
    *buf_size = min_size < 0 ? kReadBufSize : min_size;
    read_buffer_ = new net::IOBuffer(*buf_size);
  }
  *buf = read_buffer_.get();
  return true;
}

// Pass the buffer to the download file writer.
bool DownloadResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (!*bytes_read)
    return true;
  DCHECK(read_buffer_);
  AutoLock auto_lock(buffer_->lock);
  bool need_update = buffer_->contents.empty();

  // We are passing ownership of this buffer to the download file manager.
  net::IOBuffer* buffer = NULL;
  read_buffer_.swap(&buffer);
  buffer_->contents.push_back(std::make_pair(buffer, *bytes_read));
  if (need_update) {
    download_manager_->file_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(download_manager_,
                          &DownloadFileManager::UpdateDownload,
                          download_id_,
                          buffer_));
  }

  // We schedule a pause outside of the read loop if there is too much file
  // writing work to do.
  if (buffer_->contents.size() > kLoadsToWrite)
    StartPauseTimer();

  return true;
}

bool DownloadResourceHandler::OnResponseCompleted(
    int request_id,
    const URLRequestStatus& status) {
  download_manager_->file_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(download_manager_,
                        &DownloadFileManager::DownloadFinished,
                        download_id_,
                        buffer_));
  read_buffer_ = NULL;

  // 'buffer_' is deleted by the DownloadFileManager.
  buffer_ = NULL;
  return true;
}

// If the content-length header is not present (or contains something other
// than numbers), the incoming content_length is -1 (unknown size).
// Set the content length to 0 to indicate unknown size to DownloadManager.
void DownloadResourceHandler::set_content_length(const int64& content_length) {
  content_length_ = 0;
  if (content_length > 0)
    content_length_ = content_length;
}

void DownloadResourceHandler::set_content_disposition(
    const std::string& content_disposition) {
  content_disposition_ = content_disposition;
}

void DownloadResourceHandler::CheckWriteProgress() {
  if (!buffer_)
    return;  // The download completed while we were waiting to run.

  size_t contents_size;
  {
    AutoLock lock(buffer_->lock);
    contents_size = buffer_->contents.size();
  }

  bool should_pause = contents_size > kLoadsToWrite;

  // We'll come back later and see if it's okay to unpause the request.
  if (should_pause)
    StartPauseTimer();

  if (is_paused_ != should_pause) {
    rdh_->PauseRequest(global_id_.render_process_host_id,
                       global_id_.request_id,
                       should_pause);
    is_paused_ = should_pause;
  }
}

void DownloadResourceHandler::StartPauseTimer() {
  if (!pause_timer_.IsRunning())
    pause_timer_.Start(base::TimeDelta::FromMilliseconds(kThrottleTimeMs), this,
                       &DownloadResourceHandler::CheckWriteProgress);
}
