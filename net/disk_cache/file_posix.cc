// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/file.h"

#include <fcntl.h>

#include "base/logging.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

File::File(base::PlatformFile file)
    : init_(true), mixed_(true), platform_file_(file) {
}

bool File::Init(const std::wstring& name) {
  if (init_)
    return false;

  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE;
  platform_file_ = base::CreatePlatformFile(name, flags, NULL);
  if (platform_file_ < 0) {
    platform_file_ = 0;
    return false;
  }

  init_ = true;
  return true;
}

File::~File() {
  if (platform_file_)
    close(platform_file_);
}

base::PlatformFile File::platform_file() const {
  return platform_file_;
}

bool File::IsValid() const {
  if (!init_)
    return false;
  return (base::kInvalidPlatformFileValue != platform_file_);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > LONG_MAX)
    return false;

  int ret = pread(platform_file_, buffer, buffer_len, offset);
  return (static_cast<size_t>(ret) == buffer_len);
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  int ret = pwrite(platform_file_, buffer, buffer_len, offset);
  return (static_cast<size_t>(ret) == buffer_len);
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  // TODO: Implement async IO.
  bool ret = Read(buffer, buffer_len, offset);
  if (ret && completed)
    *completed = true;
  return ret;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset,
                 FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  return AsyncWrite(buffer, buffer_len, offset, true, callback, completed);
}

bool File::PostWrite(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  return AsyncWrite(buffer, buffer_len, offset, false, NULL, NULL);
}

bool File::AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                      bool notify, FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  // TODO: Implement async IO.
  bool ret = Write(buffer, buffer_len, offset);
  if (ret && completed)
    *completed = true;

  // If we supply our own async callback, and the caller is not asking to be
  // notified when completed, we are supposed to delete the buffer.
  if (ret && !callback && !notify)
    delete[] reinterpret_cast<const char*>(buffer);

  return ret;
}

bool File::SetLength(size_t length) {
  DCHECK(init_);
  if (length > ULONG_MAX)
    return false;

  return 0 == ftruncate(platform_file_, length);
}

size_t File::GetLength() {
  DCHECK(init_);
  size_t ret = lseek(platform_file_, 0, SEEK_END);
  return ret;
}

}  // namespace disk_cache
