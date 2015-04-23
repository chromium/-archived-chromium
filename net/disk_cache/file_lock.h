// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  virtual ~FileLock() {
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
