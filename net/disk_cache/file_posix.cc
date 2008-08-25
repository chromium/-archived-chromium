// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/file.h"

#include "base/logging.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

File::File(OSFile file)
    : init_(true), os_file_(0) {
}

bool File::Init(const std::wstring& name) {
  NOTIMPLEMENTED();
  return false;
}

File::~File() {
}

OSFile File::os_file() const {
  return os_file_;
}

bool File::IsValid() const {
  if (!init_)
    return false;
  return (0 != os_file_);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > LONG_MAX)
    return false;

  NOTIMPLEMENTED();
  return false;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  NOTIMPLEMENTED();
  return false;
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  NOTIMPLEMENTED();
  return false;
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

  NOTIMPLEMENTED();
  return false;
}

bool File::SetLength(size_t length) {
  DCHECK(init_);
  if (length > ULONG_MAX)
    return false;

  NOTIMPLEMENTED();
  return false;
}

size_t File::GetLength() {
  DCHECK(init_);
  return 0;
}

}  // namespace disk_cache

