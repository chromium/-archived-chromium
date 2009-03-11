// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_MAPPED_FILE_H_
#define NET_DISK_CACHE_MAPPED_FILE_H_

#include <string>

#include "base/ref_counted.h"
#include "net/disk_cache/disk_format.h"
#include "net/disk_cache/file.h"
#include "net/disk_cache/file_block.h"

namespace disk_cache {

// This class implements a memory mapped file used to access block-files. The
// idea is that the header and bitmap will be memory mapped all the time, and
// the actual data for the blocks will be access asynchronously (most of the
// time).
class MappedFile : public File {
 public:
  MappedFile() : File(true), init_(false) {}

  // Performs object initialization. name is the file to use, and size is the
  // ammount of data to memory map from th efile. If size is 0, the whole file
  // will be mapped in memory.
  void* Init(const std::wstring name, size_t size);

  void* buffer() const {
    return buffer_;
  }

  // Loads or stores a given block from the backing file (synchronously).
  bool Load(const FileBlock* block);
  bool Store(const FileBlock* block);

 protected:
  virtual ~MappedFile();

 private:
  bool init_;
#if defined(OS_WIN)
  HANDLE section_;
#endif
  void* buffer_;  // Address of the memory mapped buffer.
  size_t view_size_;  // Size of the memory pointed by buffer_.

  DISALLOW_COPY_AND_ASSIGN(MappedFile);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MAPPED_FILE_H_
