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

#include "base/shared_memory.h"

#include <fcntl.h>
#include <sys/mman.h>

#include "base/logging.h"
#include "base/string_util.h"

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
  DCHECK(mapped_file_ == -1);

  name_ = L"/" + name;
  read_only_ = read_only;

  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;
  posix_flags |= O_CREAT;
  
  int posix_mode = 0600;  // owner read/write
  std::string posix_name(WideToUTF8(name_));
  mapped_file_ = shm_open(posix_name.c_str(), posix_flags, posix_mode);
  
  if (mapped_file_ < 0)
    return false;
  ftruncate(mapped_file_, size);
  
  posix_name += kSemaphoreSuffix;
  lock_ = sem_open(posix_name.c_str(), O_CREAT, posix_mode, 1);
  if (lock_ == SEM_FAILED) {
    close(mapped_file_);
    mapped_file_ = -1;
    shm_unlink(posix_name.c_str());
    lock_ = NULL;
    return false;
  }

  max_size_ = size;
  return true;
}

bool SharedMemory::Open(const std::wstring &name, bool read_only) {
  DCHECK(mapped_file_ == -1);

  name_ = name;
  read_only_ = read_only;
  
  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;
  
  int posix_mode = 0600;  // owner read/write
  std::string posix_name(WideToUTF8(name_));
  mapped_file_ = shm_open(posix_name.c_str(), posix_flags, posix_mode);
  
  return (mapped_file_ >= 0);
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
  if (memory_ != NULL) {
    munmap(memory_, max_size_);

    memory_ = NULL;
    max_size_ = 0;
  }

  std::string posix_name(WideToUTF8(name_));
  if (mapped_file_) {
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
  sem_wait(lock_);
}

void SharedMemory::Unlock() {
  DCHECK(lock_ != NULL);
  sem_post(lock_);
}
