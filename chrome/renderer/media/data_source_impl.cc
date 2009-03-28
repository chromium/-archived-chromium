// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "chrome/renderer/media/data_source_impl.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/webmediaplayer_delegate_impl.h"
#include "chrome/renderer/render_thread.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"

DataSourceImpl::DataSourceImpl(WebMediaPlayerDelegateImpl* delegate)
    : delegate_(delegate),
      render_loop_(RenderThread::current()->message_loop()),
      stopped_(false),
      download_event_(false, false),
      downloaded_bytes_(0),
      total_bytes_(0),
      total_bytes_known_(false),
      download_completed_(false),
      resource_loader_bridge_(NULL),
      resource_release_event_(true, false),
      read_event_(false, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &DataSourceImpl::OnDidFileStreamRead)),
      stream_(NULL),
      last_read_size_(0),
      position_(0),
      io_loop_(delegate->view()->GetMessageLoopForIO()),
      seek_event_(false, false) {
  // Register ourselves with WebMediaPlayerDelgateImpl.
  delegate_->SetDataSource(this);
}

DataSourceImpl::~DataSourceImpl() {
}

void DataSourceImpl::Stop() {
  if (stopped_)
    return;
  stopped_ = true;

  // Wakes up demuxer waiting on |read_event_| in Read().
  read_event_.Signal();
  // Wakes up demuxer waiting on |seek_event_| in SetPosition().
  seek_event_.Signal();
  // Wakes up demuxer waiting on |download_event_| in Read() or SetPosition().
  download_event_.Signal();

  if (!resource_release_event_.IsSignaled()) {
    render_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DataSourceImpl::ReleaseResources, false));
    // We wait until the resource is released to make sure we won't receive
    // any call from render thread when we wake up.
    resource_release_event_.Wait();
  }
}

bool DataSourceImpl::Initialize(const std::string& url) {
  media_format_.SetAsString(media::MediaFormat::kMimeType,
                            media::mime_type::kApplicationOctetStream);
  media_format_.SetAsString(media::MediaFormat::kURL, url);
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &DataSourceImpl::OnInitialize, url));
  return true;
}

size_t DataSourceImpl::Read(uint8* data, size_t size) {
  DCHECK(stream_.get());
  // Wait until we have downloaded the requested bytes.
  while (!stopped_) {
    {
      AutoLock auto_lock(lock_);
      if (download_completed_ || position_ + size <= downloaded_bytes_)
        break;
    }
    download_event_.Wait();
  }

  last_read_size_ = media::DataSource::kReadError;
  if (!stopped_) {
    if (logging::DEBUG_MODE) {
      AutoLock auto_lock(lock_);
      DCHECK(download_completed_ || position_ + size <= downloaded_bytes_);
    }
    // Post a task to IO message loop to perform the actual reading.
    io_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DataSourceImpl::OnReadFileStream, data, size));
    read_event_.Wait();
  }
  return last_read_size_;
}

bool DataSourceImpl::GetPosition(int64* position_out) {
  AutoLock auto_lock(lock_);
  *position_out = position_;
  return true;
}

bool DataSourceImpl::SetPosition(int64 position) {
  DCHECK(stream_.get());
  while (!stopped_) {
    {
      AutoLock auto_lock(lock_);
      if (download_completed_ || position < downloaded_bytes_)
        break;
    }
    download_event_.Wait();
  }
  if (!stopped_) {
    if (logging::DEBUG_MODE) {
      AutoLock auto_lock_(lock_);
      DCHECK(download_completed_ || position < downloaded_bytes_);
    }
    // Perform the seek operation on IO message loop.
    io_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
            &DataSourceImpl::OnSeekFileStream, net::FROM_BEGIN, position));
    seek_event_.Wait();
    if (!stopped_ && logging::DEBUG_MODE) {
      AutoLock auto_lock_(lock_);
      DCHECK(position == position_);
    }
  }
  return true;
}

bool DataSourceImpl::GetSize(int64* size_out) {
  AutoLock auto_lock(lock_);
  if (!stopped_ && total_bytes_known_) {
    *size_out = total_bytes_;
    return true;
  }
  return false;
}

void DataSourceImpl::OnCreateFileStream(base::PlatformFile file) {
  stream_.reset(
      new net::FileStream(
          file, base::PLATFORM_FILE_READ | base::PLATFORM_FILE_ASYNC));
  // TODO(hclam): maybe we should check the validity of the file handle.
  host_->InitializationComplete();
}

void DataSourceImpl::OnReadFileStream(uint8* data, size_t size) {
  if (!stopped_ && stream_.get()) {
    // net::FileStream::Read wants a char*, not uint8*.
    char* c_data = reinterpret_cast<char*>(data);
    COMPILE_ASSERT(sizeof(*c_data) == sizeof(*data), data_not_sizeof_char);

    // This method IO operation is asynchronous, it is expected to return
    // ERROR_IO_PENDING, when the operation is done, OnDidFileStreamRead() will
    // be called.  Since the file handle is asynchronous, return value other
    // than ERROR_IO_PENDING is an error.
    if (stream_->Read(c_data, size, &read_callback_) != net::ERR_IO_PENDING) {
      host_->Error(media::PIPELINE_ERROR_READ);
    }
  }
}

void DataSourceImpl::OnSeekFileStream(net::Whence whence, int64 position) {
  if (!stopped_ && stream_.get()) {
    int64 new_position = stream_->Seek(whence, position);
    // Make the critical section as small as possible, because we can't assume
    // anything inside Seek() method above.
    {
      AutoLock auto_lock(lock_);
      position_ = new_position;
    }
  }
  seek_event_.Signal();
}

void DataSourceImpl::OnDidFileStreamRead(int size) {
  if (size < 0) {
    host_->Error(media::PIPELINE_ERROR_READ);
  } else {
    AutoLock auto_lock(lock_);
    // TODO(hclam): size may be an error code, handle that.
    position_ += size;
  }
  last_read_size_ = size;
  read_event_.Signal();
}

void DataSourceImpl::OnInitialize(std::string uri) {
  uri_ = uri;
  // Create the resource loader bridge.
  resource_loader_bridge_ =
      RenderThread::current()->resource_dispatcher()->CreateBridge(
      "GET",
      GURL(uri),
      GURL(uri),
      GURL(),         // TODO(hclam): provide referer here.
      "null",         // TODO(abarth): provide frame_origin
      "null",         // TODO(abarth): provide main_frame_origin
      std::string(),  // Provide no header.
      "",             // default_mime_type
      // Prefer to load from cache, also enable downloading the file, the
      // resource will be saved to a single response data file if it's possible.
      net::LOAD_PREFERRING_CACHE | net::LOAD_ENABLE_DOWNLOAD_FILE,
      base::GetCurrentProcId(),
      ResourceType::MEDIA,
      0,
      delegate_->view()->routing_id());
  // Start the resource loading.
  resource_loader_bridge_->Start(this);
}

void DataSourceImpl::ReleaseResources(bool render_thread_is_dying) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (render_thread_is_dying) {
    // If render thread is dying we can't call cancel on this pointer, we will
    // just let this object leak.
    resource_loader_bridge_ = NULL;
  } else if (resource_loader_bridge_) {
    resource_loader_bridge_->Cancel();
    resource_loader_bridge_ = NULL;
  }
  resource_release_event_.Signal();
}

void DataSourceImpl::OnDownloadProgress(uint64 position, uint64 size) {
  {
    AutoLock auto_lock(lock_);
    downloaded_bytes_ = position;
    if (!total_bytes_known_) {
      if (size == kuint64max) {
        // If we receive an invalid value for size, we keep on updating the
        // total number of bytes.
        total_bytes_ = position;
      } else {
        total_bytes_ = size;
        total_bytes_known_ = true;
      }
    }
  }
  download_event_.Signal();
}

void DataSourceImpl::OnUploadProgress(uint64 position, uint64 size) {
  // We don't care about upload progress.
}

void DataSourceImpl::OnReceivedRedirect(const GURL& new_url) {
  // TODO(hclam): what to do here? fire another resource request or show an
  // error?
}

void DataSourceImpl::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
#if defined(OS_POSIX)
  base::PlatformFile response_data_file = info.response_data_file.fd;
#elif defined(OS_WIN)
  base::PlatformFile response_data_file = info.response_data_file;
#endif

  if (response_data_file != base::kInvalidPlatformFileValue) {
    DCHECK(!position_ && !downloaded_bytes_);
    if (info.content_length != -1) {
      total_bytes_known_ = true;
      total_bytes_ = info.content_length;
    }

    // Post a task to the IO message loop to create the file stream.
    io_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DataSourceImpl::OnCreateFileStream,
                          response_data_file));
  } else {
    // TODO(hclam): handle the fallback case of using memory buffer here.
    host_->Error(media::PIPELINE_ERROR_NETWORK);
  }
}

void DataSourceImpl::OnReceivedData(const char* data, int len) {
  // TODO(hclam): we will get this method call when browser process fails
  // to provide us with a file handle, come up with some fallback mechanism.
}

void DataSourceImpl::OnCompletedRequest(const URLRequestStatus& status,
                                        const std::string& security_info) {
  {
    AutoLock auto_lock(lock_);
    total_bytes_known_ = true;
    download_completed_ = true;
  }
  if (status.status() != URLRequestStatus::SUCCESS) {
    host_->Error(media::PIPELINE_ERROR_NETWORK);
  }
}

std::string DataSourceImpl::GetURLForDebugging() {
  return uri_;
}

const media::MediaFormat& DataSourceImpl::media_format() {
  return media_format_;
}
