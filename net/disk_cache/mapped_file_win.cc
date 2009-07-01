// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mapped_file.h"

#include "base/logging.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

void* MappedFile::Init(const std::wstring& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return NULL;

  buffer_ = NULL;
  init_ = true;
  section_ = CreateFileMapping(platform_file(), NULL, PAGE_READWRITE, 0,
                               static_cast<DWORD>(size), NULL);
  if (!section_)
    return NULL;

  buffer_ = MapViewOfFile(section_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size);
  DCHECK(buffer_);
  view_size_ = size;

  return buffer_;
}

MappedFile::~MappedFile() {
  if (!init_)
    return;

  if (buffer_) {
    BOOL ret = UnmapViewOfFile(buffer_);
    DCHECK(ret);
  }

  if (section_)
    CloseHandle(section_);
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
