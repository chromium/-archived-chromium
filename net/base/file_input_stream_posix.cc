// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// For 64-bit file access (off_t = off64_t, lseek64, etc).
#define _FILE_OFFSET_BITS 64

#include "net/base/file_input_stream.h"

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

// FileInputStream::AsyncContext ----------------------------------------------

// TODO(deanm): Figure out how to best do async IO.
class FileInputStream::AsyncContext {
 public:

  CompletionCallback* callback() const { return NULL; }
 private:

  DISALLOW_COPY_AND_ASSIGN(AsyncContext);
};

// FileInputStream ------------------------------------------------------------

FileInputStream::FileInputStream() : fd_(-1) {
  DCHECK(!IsOpen());
}

FileInputStream::~FileInputStream() {
  Close();
}

void FileInputStream::Close() {
  if (fd_ != -1) {
    if (close(fd_) != 0) {
      NOTREACHED();
    }
    fd_ = -1;
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

int FileInputStream::Open(const std::wstring& path, bool asynchronous_mode) {
  // We don't need O_LARGEFILE here since we set the 64-bit off_t feature.
  fd_ = open(WideToUTF8(path).c_str(), 0, O_RDONLY);
  if (fd_ == -1)
    return MapErrorCode(errno);

  return OK;
}

bool FileInputStream::IsOpen() const {
  return fd_ != -1;
}

int64 FileInputStream::Seek(Whence whence, int64 offset) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // If we're in async, make sure we don't have a request in flight.
  DCHECK(!async_context_.get() || !async_context_->callback());
  
  off_t res = lseek(fd_, static_cast<off_t>(offset), static_cast<int>(whence));
  if (res == static_cast<off_t>(-1))
    return MapErrorCode(errno);

  return res;
}

int64 FileInputStream::Available() {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  int64 cur_pos = Seek(FROM_CURRENT, 0);
  if (cur_pos < 0)
    return cur_pos;

  struct stat info;
  if (fstat(fd_, &info) != 0)
    return MapErrorCode(errno);

  int64 size = static_cast<int64>(info.st_size);
  DCHECK(size >= cur_pos);

  return size - cur_pos;
}

// TODO(deanm): async.
int FileInputStream::Read(
    char* buf, int buf_len, CompletionCallback* callback) {
  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK(buf_len > 0 && buf_len <= SSIZE_MAX);

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // Loop in the case of getting interrupted by a signal.
  for (;;) {
    ssize_t res = read(fd_, buf, static_cast<size_t>(buf_len));
    if (res == static_cast<ssize_t>(-1)) {
      if (errno == EINTR)
        continue;
      return MapErrorCode(errno);
    }
    return static_cast<int>(res);
  }
}

}  // namespace net
