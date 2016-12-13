// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/base/file_stream.h"

#include <windows.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/net_errors.h"

namespace net {

// Ensure that we can just use our Whence values directly.
COMPILE_ASSERT(FROM_BEGIN == FILE_BEGIN, bad_whence_begin);
COMPILE_ASSERT(FROM_CURRENT == FILE_CURRENT, bad_whence_current);
COMPILE_ASSERT(FROM_END == FILE_END, bad_whence_end);

static void SetOffset(OVERLAPPED* overlapped, const LARGE_INTEGER& offset) {
  overlapped->Offset = offset.LowPart;
  overlapped->OffsetHigh = offset.HighPart;
}

static void IncrementOffset(OVERLAPPED* overlapped, DWORD count) {
  LARGE_INTEGER offset;
  offset.LowPart = overlapped->Offset;
  offset.HighPart = overlapped->OffsetHigh;
  offset.QuadPart += static_cast<LONGLONG>(count);
  SetOffset(overlapped, offset);
}

static int MapErrorCode(DWORD err) {
  switch (err) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return ERR_FILE_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
      return ERR_ACCESS_DENIED;
    case ERROR_SUCCESS:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// FileStream::AsyncContext ----------------------------------------------

class FileStream::AsyncContext : public MessageLoopForIO::IOHandler {
 public:
  AsyncContext(FileStream* owner)
      : owner_(owner), context_(), callback_(NULL), is_closing_(false) {
    context_.handler = this;
  }
  ~AsyncContext();

  void IOCompletionIsPending(CompletionCallback* callback);

  OVERLAPPED* overlapped() { return &context_.overlapped; }
  CompletionCallback* callback() const { return callback_; }

 private:
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_read, DWORD error);

  FileStream* owner_;
  MessageLoopForIO::IOContext context_;
  CompletionCallback* callback_;
  bool is_closing_;
};

FileStream::AsyncContext::~AsyncContext() {
  is_closing_ = true;
  bool waited = false;
  base::Time start = base::Time::Now();
  while (callback_) {
    waited = true;
    MessageLoopForIO::current()->WaitForIOCompletion(INFINITE, this);
  }
  if (waited) {
    // We want to see if we block the message loop for too long.
    UMA_HISTOGRAM_TIMES("AsyncIO.FileStreamClose", base::Time::Now() - start);
  }
}

void FileStream::AsyncContext::IOCompletionIsPending(
    CompletionCallback* callback) {
  DCHECK(!callback_);
  callback_ = callback;
}

void FileStream::AsyncContext::OnIOCompleted(
    MessageLoopForIO::IOContext* context, DWORD bytes_read, DWORD error) {
  DCHECK(&context_ == context);
  DCHECK(callback_);

  if (is_closing_) {
    callback_ = NULL;
    return;
  }

  int result = static_cast<int>(bytes_read);
  if (error && error != ERROR_HANDLE_EOF)
    result = MapErrorCode(error);

  if (bytes_read)
    IncrementOffset(&context->overlapped, bytes_read);

  CompletionCallback* temp = NULL;
  std::swap(temp, callback_);
  temp->Run(result);
}

// FileStream ------------------------------------------------------------

FileStream::FileStream() : file_(INVALID_HANDLE_VALUE) {
}

FileStream::FileStream(base::PlatformFile file, int flags)
    : file_(file), open_flags_(flags) {
  // If the file handle is opened with base::PLATFORM_FILE_ASYNC, we need to
  // make sure we will perform asynchronous File IO to it.
  if (flags & base::PLATFORM_FILE_ASYNC) {
    async_context_.reset(new AsyncContext(this));
    MessageLoopForIO::current()->RegisterIOHandler(file_,
                                                   async_context_.get());
  }
}

FileStream::~FileStream() {
  Close();
}

void FileStream::Close() {
  if (file_ != INVALID_HANDLE_VALUE)
    CancelIo(file_);

  async_context_.reset();
  if (file_ != INVALID_HANDLE_VALUE) {
    CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
  }
}

int FileStream::Open(const FilePath& path, int open_flags) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  open_flags_ = open_flags;
  file_ = base::CreatePlatformFile(path.value(), open_flags_, NULL);
  if (file_ == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    LOG(WARNING) << "Failed to open file: " << error;
    return MapErrorCode(error);
  }

  if (open_flags_ & base::PLATFORM_FILE_ASYNC) {
    async_context_.reset(new AsyncContext(this));
    MessageLoopForIO::current()->RegisterIOHandler(file_,
                                                   async_context_.get());
  }

  return OK;
}

bool FileStream::IsOpen() const {
  return file_ != INVALID_HANDLE_VALUE;
}

int64 FileStream::Seek(Whence whence, int64 offset) {
  if (!IsOpen())
    return ERR_UNEXPECTED;
  DCHECK(!async_context_.get() || !async_context_->callback());

  LARGE_INTEGER distance, result;
  distance.QuadPart = offset;
  DWORD move_method = static_cast<DWORD>(whence);
  if (!SetFilePointerEx(file_, distance, &result, move_method)) {
    DWORD error = GetLastError();
    LOG(WARNING) << "SetFilePointerEx failed: " << error;
    return MapErrorCode(error);
  }
  if (async_context_.get())
    SetOffset(async_context_->overlapped(), result);
  return result.QuadPart;
}

int64 FileStream::Available() {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  int64 cur_pos = Seek(FROM_CURRENT, 0);
  if (cur_pos < 0)
    return cur_pos;

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file_, &file_size)) {
    DWORD error = GetLastError();
    LOG(WARNING) << "GetFileSizeEx failed: " << error;
    return MapErrorCode(error);
  }

  return file_size.QuadPart - cur_pos;
}

int FileStream::Read(
    char* buf, int buf_len, CompletionCallback* callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;
  DCHECK(open_flags_ & base::PLATFORM_FILE_READ);

  OVERLAPPED* overlapped = NULL;
  if (async_context_.get()) {
    DCHECK(!async_context_->callback());
    overlapped = async_context_->overlapped();
  }

  int rv;

  DWORD bytes_read;
  if (!ReadFile(file_, buf, buf_len, &bytes_read, overlapped)) {
    DWORD error = GetLastError();
    if (async_context_.get() && error == ERROR_IO_PENDING) {
      async_context_->IOCompletionIsPending(callback);
      rv = ERR_IO_PENDING;
    } else if (error == ERROR_HANDLE_EOF) {
      rv = 0;  // Report EOF by returning 0 bytes read.
    } else {
      LOG(WARNING) << "ReadFile failed: " << error;
      rv = MapErrorCode(error);
    }
  } else if (overlapped) {
    async_context_->IOCompletionIsPending(callback);
    rv = ERR_IO_PENDING;
  } else {
    rv = static_cast<int>(bytes_read);
  }
  return rv;
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

int FileStream::Write(
    const char* buf, int buf_len, CompletionCallback* callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;
  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);

  OVERLAPPED* overlapped = NULL;
  if (async_context_.get()) {
    DCHECK(!async_context_->callback());
    overlapped = async_context_->overlapped();
  }

  int rv;
  DWORD bytes_written;
  if (!WriteFile(file_, buf, buf_len, &bytes_written, overlapped)) {
    DWORD error = GetLastError();
    if (async_context_.get() && error == ERROR_IO_PENDING) {
      async_context_->IOCompletionIsPending(callback);
      rv = ERR_IO_PENDING;
    } else {
      LOG(WARNING) << "WriteFile failed: " << error;
      rv = MapErrorCode(error);
    }
  } else if (overlapped) {
    async_context_->IOCompletionIsPending(callback);
    rv = ERR_IO_PENDING;
  } else {
    rv = static_cast<int>(bytes_written);
  }
  return rv;
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
  BOOL result = SetEndOfFile(file_);
  if (!result) {
    DWORD error = GetLastError();
    LOG(WARNING) << "SetEndOfFile failed: " << error;
    return MapErrorCode(error);
  }

  // Success.
  return seek_position;
}

}  // namespace net
