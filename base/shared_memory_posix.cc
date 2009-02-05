// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_thread.h"
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
      max_size_(0) {
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only)
    : mapped_file_(handle),
      memory_(NULL),
      read_only_(read_only),
      max_size_(0) {
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only,
                           ProcessHandle process)
    : mapped_file_(handle),
      memory_(NULL),
      read_only_(read_only),
      max_size_(0) {
  // We don't handle this case yet (note the ignored parameter); let's die if
  // someone comes calling.
  NOTREACHED();
}

SharedMemory::~SharedMemory() {
  Close();
}

bool SharedMemory::Create(const std::wstring &name, bool read_only,
                          bool open_existing, size_t size) {
  read_only_ = read_only;

  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;
  if (!open_existing || mapped_file_ <= 0)
    posix_flags |= O_CREAT;

  if (!CreateOrOpen(name, posix_flags, size))
    return false;

  max_size_ = size;
  return true;
}

// Our current implementation of shmem is with mmap()ing of files.
// These files need to be deleted explicitly.
// In practice this call is only needed for unit tests.
bool SharedMemory::Delete(const std::wstring& name) {
  std::wstring mem_filename;
  if (FilenameForMemoryName(name, &mem_filename) == false)
    return false;

  FilePath path(WideToUTF8(mem_filename));
  if (file_util::PathExists(path)) {
    return file_util::Delete(path, false);
  }

  // Doesn't exist, so success.
  return true;
}

bool SharedMemory::Open(const std::wstring &name, bool read_only) {
  read_only_ = read_only;

  int posix_flags = 0;
  posix_flags |= read_only ? O_RDONLY : O_RDWR;

  return CreateOrOpen(name, posix_flags, 0);
}

// For the given shmem named |memname|, return a filename to mmap()
// (and possibly create).  Modifies |filename|.  Return false on
// error, or true of we are happy.
bool SharedMemory::FilenameForMemoryName(const std::wstring &memname,
                                         std::wstring *filename) {
  std::wstring mem_filename;

  // mem_name will be used for a filename; make sure it doesn't
  // contain anything which will confuse us.
  DCHECK(memname.find_first_of(L"/") == std::string::npos);
  DCHECK(memname.find_first_of(L"\0") == std::string::npos);

  FilePath temp_dir;
  if (file_util::GetShmemTempDir(&temp_dir) == false)
    return false;

  mem_filename = UTF8ToWide(temp_dir.value());
  file_util::AppendToPath(&mem_filename, L"com.google.chrome.shmem." + memname);
  *filename = mem_filename;
  return true;
}

// Current expectation is that Cromium only really needs
// unique/private shmem as specified by "name == L"".
// TODO(port): confirm that assumption.
// TODO(jrg): there is no way to "clean up" all unused named shmem if
// we restart from a crash.  (That isn't a new problem, but it is a problem.)
bool SharedMemory::CreateOrOpen(const std::wstring &name,
                                int posix_flags, size_t size) {
  DCHECK(mapped_file_ == -1);

  file_util::ScopedFILE file_closer;
  FILE *fp;

  if (name == L"") {
    // It doesn't make sense to have a read-only private piece of shmem
    DCHECK(posix_flags & (O_RDWR | O_WRONLY));

    FilePath path;
    fp = file_util::CreateAndOpenTemporaryShmemFile(&path);
    name_ = UTF8ToWide(path.value());

    // Deleting the file prevents anyone else from mapping it in
    // (making it private), and prevents the need for cleanup (once
    // the last fd is closed, it is truly freed).
    file_util::Delete(path, false);
  } else {
    std::wstring mem_filename;
    if (FilenameForMemoryName(name, &mem_filename) == false)
      return false;

    name_ = mem_filename;
    std::string mode;
    switch (posix_flags) {
      case (O_RDWR | O_CREAT):
        // Careful: "w+" will truncate if it already exists.
        mode = "a+";
        break;
      case O_RDWR:
        mode = "r+";
        break;
      case O_RDONLY:
        mode = "r";
        break;
      default:
        NOTIMPLEMENTED();
        break;
    }

    fp = file_util::OpenFile(mem_filename, mode.c_str());
  }

  if (fp == NULL)
    return false;
  file_closer.reset(fp);  // close when we go out of scope

  // Make sure the (new) file is the right size.
  // According to the man page, "Use of truncate() to extend a file is
  // not portable."
  if (size && (posix_flags & (O_RDWR | O_CREAT))) {
    // Get current size.
    size_t current_size = 0;
    struct stat stat;
    if (fstat(fileno(fp), &stat) != 0)
      return false;
    current_size = stat.st_size;
    // Possibly grow.
    if (current_size < size) {
      if (fseeko(fp, current_size, SEEK_SET) != 0)
        return false;
      size_t writesize = size - current_size;
      scoped_array<char> buf(new char[writesize]);
      memset(buf.get(), 0, writesize);
      if (fwrite(buf.get(), 1, writesize, fp) != writesize) {
          return false;
      }
      if (fflush(fp) != 0)
        return false;
    } else if (current_size > size) {
      // possibly shrink.
      if ((ftruncate(fileno(fp), size) != 0) ||
          (fflush(fp) != 0)) {
        return false;
      }
    }
  }

  mapped_file_ = dup(fileno(fp));
  DCHECK(mapped_file_ >= 0);
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
    mapped_file_ = -1;
  }
}

void SharedMemory::LockOrUnlockCommon(int function) {
  DCHECK(mapped_file_ >= 0);
  while (lockf(mapped_file_, function, 0) < 0) {
    if (errno == EINTR) {
      continue;
    } else if (errno == ENOLCK) {
      // temporary kernel resource exaustion
      PlatformThread::Sleep(500);
      continue;
    } else {
      NOTREACHED() << "lockf() failed."
                   << " function:" << function
                   << " fd:" << mapped_file_
                   << " errno:" << errno
                   << " msg:" << strerror(errno);
    }
  }
}

void SharedMemory::Lock() {
  LockOrUnlockCommon(F_LOCK);
}

void SharedMemory::Unlock() {
  LockOrUnlockCommon(F_ULOCK);
}

}  // namespace base
