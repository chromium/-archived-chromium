// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "base/logging.h"
#include "base/string_util.h"

namespace base {

namespace {
// Paranoia. Semaphores and shared memory segments should live in different
// namespaces, but who knows what's out there.
const char kSemaphoreSuffix[] = "-sem";
}

SharedMemory::SharedMemory()
    : mapped_file_(-1),
      memory_(NULL),
      read_only_(false),
      max_size_(0),
      lock_(NULL) {
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only)
    : mapped_file_(handle),
      memory_(NULL),
      read_only_(read_only),
      max_size_(0),
      lock_(NULL) {
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only,
                           ProcessHandle process)
    : mapped_file_(handle),
      memory_(NULL),
      read_only_(read_only),
      max_size_(0),
      lock_(NULL) {
  // We don't handle this case yet (note the ignored parameter); let's die if
  // someone comes calling.
  NOTREACHED();
}

SharedMemory::~SharedMemory() {
  Close();
  if (lock_ != NULL)
    sem_close(lock_);
}

bool SharedMemory::Create(const std::wstring &name, bool read_only,
                          bool open_existing, size_t size) {
  read_only_ = read_only;

  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;
  if (!open_existing || mapped_file_ <= 0)
    posix_flags |= O_CREAT;

  if (!CreateOrOpen(name, posix_flags))
    return false;

  if (ftruncate(mapped_file_, size) != 0) {
    Close();
    return false;
  }

  max_size_ = size;
  return true;
}

bool SharedMemory::Open(const std::wstring &name, bool read_only) {
  read_only_ = read_only;

  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;

  return CreateOrOpen(name, posix_flags);
}

bool SharedMemory::CreateOrOpen(const std::wstring &name, int posix_flags) {
  DCHECK(mapped_file_ == -1);

  name_ = L"/" + name;

  mode_t posix_mode = S_IRUSR | S_IWUSR;  // owner read/write
  std::string posix_name(WideToUTF8(name_));
  mapped_file_ = shm_open(posix_name.c_str(), posix_flags, posix_mode);
  if (mapped_file_ < 0)
    return false;

  posix_name += kSemaphoreSuffix;
  lock_ = sem_open(posix_name.c_str(), O_CREAT, posix_mode, 1);
  if (lock_ == SEM_FAILED) {
    close(mapped_file_);
    mapped_file_ = -1;
    shm_unlink(posix_name.c_str());
    lock_ = NULL;
    return false;
  }

  return true;
}

bool SharedMemory::Map(size_t bytes) {
  if (mapped_file_ == -1)
    return false;

  memory_ = mmap(NULL, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, mapped_file_, 0);

  if (memory_)
    max_size_ = bytes;

  return (memory_ != NULL);
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL)
    return false;

  munmap(memory_, max_size_);
  memory_ = NULL;
  max_size_ = 0;
  return true;
}

bool SharedMemory::ShareToProcessCommon(ProcessHandle process,
                                        SharedMemoryHandle *new_handle,
                                        bool close_self) {
  *new_handle = 0;
  // TODO(awalker): figure out if we need this, and do the appropriate
  // VM magic if so.
  return false;
}


void SharedMemory::Close() {

  Unmap();

  std::string posix_name(WideToUTF8(name_));
  if (mapped_file_ > 0) {
    close(mapped_file_);
    shm_unlink(posix_name.c_str());
    mapped_file_ = -1;
  }

  if (lock_) {
    posix_name += kSemaphoreSuffix;
    sem_unlink(posix_name.c_str());
    lock_ = NULL;
  }
}

void SharedMemory::Lock() {
  DCHECK(lock_ != NULL);
  while(sem_wait(lock_) < 0) {
    DCHECK(errno == EAGAIN || errno == EINTR);
  }
}

void SharedMemory::Unlock() {
  DCHECK(lock_ != NULL);
  int result = sem_post(lock_);
  DCHECK(result == 0);
}

}  // namespace base
