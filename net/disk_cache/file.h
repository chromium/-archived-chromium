// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_FILE_H_
#define NET_DISK_CACHE_FILE_H_

#include <string>

#include "base/platform_file.h"
#include "base/ref_counted.h"

namespace disk_cache {

// This interface is used to support asynchronous ReadData and WriteData calls.
class FileIOCallback {
 public:
  // Notified of the actual number of bytes read or written. This value is
  // negative if an error occurred.
  virtual void OnFileIOComplete(int bytes_copied) = 0;

  virtual ~FileIOCallback() {}
};

// Simple wrapper around a file that allows asynchronous operations.
class File : public base::RefCounted<File> {
  friend class base::RefCounted<File>;
 public:
  File() : init_(false), mixed_(false) {}
  // mixed_mode set to true enables regular synchronous operations for the file.
  explicit File(bool mixed_mode) : init_(false), mixed_(mixed_mode) {}

  // Initializes the object to use the passed in file instead of opening it with
  // the Init() call. No asynchronous operations can be performed with this
  // object.
  explicit File(base::PlatformFile file);

  // Initializes the object to point to a given file. The file must aready exist
  // on disk, and allow shared read and write.
  bool Init(const std::wstring& name);

  // Returns the handle or file descriptor.
  base::PlatformFile platform_file() const;

  // Returns true if the file was opened properly.
  bool IsValid() const;

  // Performs synchronous IO.
  bool Read(void* buffer, size_t buffer_len, size_t offset);
  bool Write(const void* buffer, size_t buffer_len, size_t offset);

  // Performs asynchronous IO. callback will be called when the IO completes,
  // as an APC on the thread that queued the operation.
  bool Read(void* buffer, size_t buffer_len, size_t offset,
            FileIOCallback* callback, bool* completed);
  bool Write(const void* buffer, size_t buffer_len, size_t offset,
             FileIOCallback* callback, bool* completed);

  // Performs asynchronous writes, but doesn't notify when done. Automatically
  // deletes buffer when done.
  bool PostWrite(const void* buffer, size_t buffer_len, size_t offset);

  // Sets the file's length. The file is truncated or extended with zeros to
  // the new length.
  bool SetLength(size_t length);
  size_t GetLength();

 protected:
  virtual ~File();

  // Performs the actual asynchronous write. If notify is set and there is no
  // callback, the call will be re-synchronized.
  bool AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                  bool notify, FileIOCallback* callback, bool* completed);

 private:
  bool init_;
  bool mixed_;
  base::PlatformFile platform_file_;  // Regular, asynchronous IO handle.
  base::PlatformFile sync_platform_file_;  // Synchronous IO handle.

  DISALLOW_COPY_AND_ASSIGN(File);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FILE_H_
