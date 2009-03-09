// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// For 64-bit file access (off_t = off64_t, lseek64, etc).
#define _FILE_OFFSET_BITS 64

#include "net/base/file_stream.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"

// We cast back and forth, so make sure it's the size we're expecting.
COMPILE_ASSERT(sizeof(int64) == sizeof(off_t), off_t_64_bit);

// Make sure our Whence mappings match the system headers.
COMPILE_ASSERT(net::FROM_BEGIN   == SEEK_SET &&
               net::FROM_CURRENT == SEEK_CUR &&
               net::FROM_END     == SEEK_END, whence_matches_system);

namespace net {

// FileStream::AsyncContext ----------------------------------------------

// TODO(deanm): Figure out how to best do async IO.
class FileStream::AsyncContext {
 public:

  CompletionCallback* callback() const { return NULL; }
 private:

  DISALLOW_COPY_AND_ASSIGN(AsyncContext);
};

// FileStream ------------------------------------------------------------

FileStream::FileStream() : file_(base::kInvalidPlatformFileValue) {
  DCHECK(!IsOpen());
}

FileStream::FileStream(base::PlatformFile file, int flags)
    : file_(file), open_flags_(flags) {
  // TODO(hclam): initialize the aync_context_ if the file handle
  // is opened as an asynchronous file handle.
}

FileStream::~FileStream() {
  Close();
}

void FileStream::Close() {
  if (file_ != base::kInvalidPlatformFileValue) {
    if (close(file_) != 0) {
      NOTREACHED();
    }
    file_ = base::kInvalidPlatformFileValue;
  }
  async_context_.reset();
}

// Map from errno to net error codes.
static int64 MapErrorCode(int err) {
  switch(err) {
    case ENOENT:
      return ERR_FILE_NOT_FOUND;
    case EACCES:
      return ERR_ACCESS_DENIED;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

int FileStream::Open(const FilePath& path, int open_flags) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  open_flags_ = open_flags;
  file_ = base::CreatePlatformFile(path.ToWStringHack(), open_flags_, NULL);
  if (file_ == base::kInvalidPlatformFileValue) {
    LOG(WARNING) << "Failed to open file: " << errno;
    return MapErrorCode(errno);
  }

  return OK;
}

bool FileStream::IsOpen() const {
  return file_ != base::kInvalidPlatformFileValue;
}

int64 FileStream::Seek(Whence whence, int64 offset) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // If we're in async, make sure we don't have a request in flight.
  DCHECK(!async_context_.get() || !async_context_->callback());

  off_t res = lseek(file_, static_cast<off_t>(offset),
                    static_cast<int>(whence));
  if (res == static_cast<off_t>(-1))
    return MapErrorCode(errno);

  return res;
}

int64 FileStream::Available() {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  int64 cur_pos = Seek(FROM_CURRENT, 0);
  if (cur_pos < 0)
    return cur_pos;

  struct stat info;
  if (fstat(file_, &info) != 0)
    return MapErrorCode(errno);

  int64 size = static_cast<int64>(info.st_size);
  DCHECK(size >= cur_pos);

  return size - cur_pos;
}

// TODO(deanm): async.
int FileStream::Read(
    char* buf, int buf_len, CompletionCallback* callback) {
  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK(buf_len > 0 && buf_len <= SSIZE_MAX);

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // Loop in the case of getting interrupted by a signal.
  for (;;) {
    ssize_t res = read(file_, buf, static_cast<size_t>(buf_len));
    if (res == static_cast<ssize_t>(-1)) {
      if (errno == EINTR)
        continue;
      return MapErrorCode(errno);
    }
    return static_cast<int>(res);
  }
}

int FileStream::ReadUntilComplete(char *buf, int buf_len) {
  int to_read = buf_len;
  int bytes_total = 0;

  do {
    int bytes_read = Read(buf, to_read, NULL);
    if (bytes_read <= 0) {
      if (bytes_total == 0)
        return bytes_read;

      return bytes_total;
    }

    bytes_total += bytes_read;
    buf += bytes_read;
    to_read -= bytes_read;
  } while (bytes_total < buf_len);

  return bytes_total;
}

// TODO(deanm): async.
int FileStream::Write(
    const char* buf, int buf_len, CompletionCallback* callback) {

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK(buf_len > 0 && buf_len <= SSIZE_MAX);

  if (!IsOpen())
    return ERR_UNEXPECTED;

  int total_bytes_written = 0;
  size_t len = static_cast<size_t>(buf_len);
  while (total_bytes_written < buf_len) {
    ssize_t res = write(file_, buf, len);
    if (res == static_cast<ssize_t>(-1)) {
      if (errno == EINTR)
        continue;
      return MapErrorCode(errno);
    }
    total_bytes_written += res;
    buf += res;
    len -= res;
  }
  return total_bytes_written;
}

int64 FileStream::Truncate(int64 bytes) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // We better be open for reading.
  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);

  // Seek to the position to truncate from.
  int64 seek_position = Seek(FROM_BEGIN, bytes);
  if (seek_position != bytes)
    return ERR_UNEXPECTED;

  // And truncate the file.
  int result = ftruncate(file_, bytes);
  return result == 0 ? seek_position : MapErrorCode(errno);
}

}  // namespace net
