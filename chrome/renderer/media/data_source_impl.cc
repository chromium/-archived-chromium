// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/renderer/media/data_source_impl.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/webmediaplayer_delegate_impl.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline.h"
#include "net/base/net_errors.h"

DataSourceImpl::DataSourceImpl(WebMediaPlayerDelegateImpl* delegate)
    : delegate_(delegate),
      stopped_(false),
      download_event_(false, false),
      downloaded_bytes_(0),
      total_bytes_(0),
      total_bytes_known_(false),
      read_event_(false, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &DataSourceImpl::OnDidFileStreamRead)),
      stream_(NULL),
      last_read_size_(0),
      position_(0),
      io_loop_(delegate->view()->GetMessageLoopForIO()),
      close_event_(false, false),
      seek_event_(false, false) {
}

DataSourceImpl::~DataSourceImpl() {
}

void DataSourceImpl::Stop() {
  if (stopped_)
    return;
  stopped_ = true;

  // TODO(hclam): things to do here:
  // 1. Call to RenderView to cancel the streaming resource request.
  // 2. Signal download_event_.
  download_event_.Signal();

  // Post a close file stream task to IO message loop, it will signal the read
  // event.
  io_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &DataSourceImpl::OnCloseFileStream));

  // Wait for close to finish for FileStream.
  close_event_.Wait();

  // Make sure that when we wake up the stream is closed and destroyed.
  DCHECK(stream_.get() == NULL);
}

bool DataSourceImpl::Initialize(const std::string& url) {
  // TODO(hclam): call to RenderView by delegate_->view() to initiate a
  // streaming session in the browser process. Call to RenderView by
  // delegate_->view() to initiate a streaming session in the browser process.
  // We should get a call back at OnReceivedResponse().
  media_format_.SetAsString(media::MediaFormat::kMimeType,
                            media::mime_type::kApplicationOctetStream);
  media_format_.SetAsString(media::MediaFormat::kURL, url);
  return true;
}

size_t DataSourceImpl::Read(uint8* data, size_t size) {
  DCHECK(stream_.get());
  // Wait until we have downloaded the requested bytes.
  while (!stopped_) {
    {
      AutoLock auto_lock(lock_);
      if (position_ + size <= downloaded_bytes_)
        break;
    }
    download_event_.Wait();
  }

  last_read_size_ = media::DataSource::kReadError;
  if (!stopped_) {
    if (logging::DEBUG_MODE) {
      AutoLock auto_lock(lock_);
      DCHECK(position_ + size <= downloaded_bytes_);
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
      if (position < downloaded_bytes_)
        break;
    }
    download_event_.Wait();
  }
  if (!stopped_) {
    if (logging::DEBUG_MODE) {
      AutoLock auto_lock_(lock_);
      DCHECK(position < downloaded_bytes_);
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

void DataSourceImpl::OnCloseFileStream() {
  if (stream_.get()) {
    stream_->Close();
    stream_.reset();
  }
  // Wakes up demuxer waiting on read_event_ in Read().
  read_event_.Signal();
  // Wakes up demuxer waiting on seek_event_ in SetPosition().
  seek_event_.Signal();
  // Wakes up pipeline thread blocked in Stop() by close_event_.
  close_event_.Signal();
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
  {
    AutoLock auto_lock(lock_);
    last_read_size_ = size;
    // TODO(hclam): size may be an error code, handle that.
    position_ += size;
  }
  read_event_.Signal();
}

void DataSourceImpl::OnReceivedResponse(base::PlatformFile file,
                                        int response_code,
                                        int64 content_length) {
  // TODO(hclam): come up with a valid set of response codes and react to the
  // code properly and not just the file handle.
  if (file != base::kInvalidPlatformFileValue) {
    DCHECK(!position_ && !downloaded_bytes_);
    total_bytes_ = content_length;
    if (content_length != 0)
      total_bytes_known_ = true;

    // Post a task to the IO message loop to create the file stream.
    delegate_->view()->GetMessageLoopForIO()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DataSourceImpl::OnCreateFileStream, file));
  } else {
    host_->Error(media::PIPELINE_ERROR_NETWORK);
  }
}

void DataSourceImpl::OnReceivedData(size_t bytes) {
  {
    AutoLock auto_lock(lock_);
    downloaded_bytes_ += bytes;
    if (!total_bytes_known_)
      total_bytes_ += bytes;
  }
  download_event_.Signal();
}

const media::MediaFormat* DataSourceImpl::GetMediaFormat() {
  return &media_format_;
}
