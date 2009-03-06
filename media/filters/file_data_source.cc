// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/string_util.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/pipeline.h"
#include "media/filters/file_data_source.h"

namespace media {

FileDataSource::FileDataSource()
    : file_(NULL),
      file_size_(0) {
}

FileDataSource::~FileDataSource() {
  Stop();
}

bool FileDataSource::Initialize(const std::string& url) {
  DCHECK(!file_);
#if defined(OS_WIN)
  FilePath file_path(UTF8ToWide(url));
#else
  FilePath file_path(url);
#endif
  if (file_util::GetFileSize(file_path, &file_size_)) {
    file_ = file_util::OpenFile(file_path, "rb");
  }
  if (!file_) {
    file_size_ = 0;
    host_->Error(PIPELINE_ERROR_URL_NOT_FOUND);
    return false;
  }
  media_format_.SetAsString(MediaFormat::kMimeType,
                            mime_type::kApplicationOctetStream);
  media_format_.SetAsString(MediaFormat::kURL, url);
  host_->SetTotalBytes(file_size_);
  host_->SetBufferedBytes(file_size_);
  host_->InitializationComplete();
  return true;
}

void FileDataSource::Stop() {
  AutoLock l(lock_);
  if (file_) {
    file_util::CloseFile(file_);
    file_ = NULL;
    file_size_ = 0;
  }
}

const MediaFormat* FileDataSource::GetMediaFormat() {
  return &media_format_;
}

size_t FileDataSource::Read(uint8* data, size_t size) {
  DCHECK(file_);
  AutoLock l(lock_);
  if (file_) {
    size_t size_read = fread(data, 1, size, file_);
    if (size_read == size || !ferror(file_)) {
      return size_read;
    }
  }
  return kReadError;
}

bool FileDataSource::GetPosition(int64* position_out) {
  DCHECK(position_out);
  DCHECK(file_);
  AutoLock l(lock_);
  if (!file_) {
    *position_out = 0;
    return false;
  }
// Linux and mac libraries don't seem to support 64 versions of seek and
// ftell.  TODO(ralph): Try to figure out how to enable int64 versions on
// these platforms.
#if defined(OS_WIN)
  *position_out = _ftelli64(file_);
#else
  *position_out = ftell(file_);
#endif
  return true;
}

bool FileDataSource::SetPosition(int64 position) {
  DCHECK(file_);
  AutoLock l(lock_);
#if defined(OS_WIN)
  if (file_ && 0 == _fseeki64(file_, position, SEEK_SET)) {
    return true;
  }
#else
  if (file_ && 0 == fseek(file_, static_cast<int32>(position), SEEK_SET)) {
    return true;
  }
#endif
  return false;
}

bool FileDataSource::GetSize(int64* size_out) {
  DCHECK(size_out);
  DCHECK(file_);
  AutoLock l(lock_);
  *size_out = file_size_;
  return (NULL != file_);
}

}  // namespace media
