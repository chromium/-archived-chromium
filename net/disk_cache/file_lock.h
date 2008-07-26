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

#ifndef NET_DISK_CACHE_FILE_LOCK_H__
#define NET_DISK_CACHE_FILE_LOCK_H__

#include "net/disk_cache/disk_format.h"

namespace disk_cache {

// This class implements a file lock that lives on the header of a memory mapped
// file. This is NOT a thread related lock, it is a lock to detect corruption
// of the file when the process crashes in the middle of an update.
// The lock is acquired on the constructor and released on the destructor.
// The typical use of the class is:
//    {
//      BlockFileHeader* header = GetFileHeader();
//      FileLock lock(header);
//      header->max_entries = num_entries;
//      // At this point the destructor is going to release the lock.
//    }
// It is important to perform Lock() and Unlock() operations in the right order,
// because otherwise the desired effect of the "lock" will not be achieved. If
// the operations are inlined / optimized, the "locked" operations can happen
// outside the lock.
class FileLock {
 public:
  explicit FileLock(BlockFileHeader* header);
  ~FileLock() {
    Unlock();
  }
  // Virtual to make sure the compiler never inlines the calls.
  virtual void Lock();
  virtual void Unlock();
 private:
  bool acquired_;
  volatile int32* updating_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FILE_LOCK_H__
