// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
  MappedFile() : init_(false), File(true) {}

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
