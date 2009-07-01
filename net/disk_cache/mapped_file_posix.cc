// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mapped_file.h"

#include <errno.h>
#include <sys/mman.h>

#include "base/logging.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

void* MappedFile::Init(const std::wstring& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return NULL;

  if (!size)
    size = GetLength();

  buffer_ = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                 platform_file(), 0);
  init_ = true;
  DCHECK(reinterpret_cast<int>(buffer_) != -1);
  if (reinterpret_cast<int>(buffer_) == -1)
    buffer_ = 0;

  view_size_ = size;
  return buffer_;
}

MappedFile::~MappedFile() {
  if (!init_)
    return;

  if (buffer_) {
    int ret = munmap(buffer_, view_size_);
    DCHECK(0 == ret);
  }
}

bool MappedFile::Load(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Read(block->buffer(), block->size(), offset);
}

bool MappedFile::Store(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Write(block->buffer(), block->size(), offset);
}

}  // namespace disk_cache
