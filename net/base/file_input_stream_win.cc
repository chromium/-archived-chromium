// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/base/file_input_stream.h"

#include <windows.h>

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

// FileInputStream::AsyncContext ----------------------------------------------

class FileInputStream::AsyncContext : public MessageLoopForIO::IOHandler {
 public:
  AsyncContext(FileInputStream* owner)
      : owner_(owner), overlapped_(), callback_(NULL) {
    overlapped_.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  }

  ~AsyncContext() {
    if (callback_)
      MessageLoopForIO::current()->RegisterIOContext(&overlapped_, NULL);
    CloseHandle(overlapped_.hEvent);
  }

  void IOCompletionIsPending(CompletionCallback* callback);
  
  OVERLAPPED* overlapped() { return &overlapped_; }
  CompletionCallback* callback() const { return callback_; }

 private:
  // MessageLoopForIO::IOHandler implementation:
  virtual void OnIOCompleted(OVERLAPPED* context, DWORD bytes_read,
                             DWORD error);

  FileInputStream* owner_;
  OVERLAPPED overlapped_;
  CompletionCallback* callback_;
};

void FileInputStream::AsyncContext::IOCompletionIsPending(
    CompletionCallback* callback) {
  DCHECK(!callback_);
  callback_ = callback;

  MessageLoopForIO::current()->RegisterIOContext(&overlapped_, this);
}

void FileInputStream::AsyncContext::OnIOCompleted(OVERLAPPED* context,
                                                  DWORD bytes_read,
                                                  DWORD error) {
  DCHECK(&overlapped_ == context);
  DCHECK(callback_);

  MessageLoopForIO::current()->RegisterIOContext(&overlapped_, NULL);

  HANDLE handle = owner_->handle_;

  int result = static_cast<int>(bytes_read);
  if (error && error != ERROR_HANDLE_EOF)
    result = MapErrorCode(error);
  
  if (bytes_read)
    IncrementOffset(&overlapped_, bytes_read);

  CompletionCallback* temp = NULL;
  std::swap(temp, callback_);
  temp->Run(result);
}

// FileInputStream ------------------------------------------------------------

FileInputStream::FileInputStream() : handle_(INVALID_HANDLE_VALUE) {
}

FileInputStream::~FileInputStream() {
  Close();
}

void FileInputStream::Close() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
  }
  async_context_.reset();
}

int FileInputStream::Open(const std::wstring& path, bool asynchronous_mode) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  // Optimize for streaming, not seeking.  If someone does a lot of random
  // access operations, then we should consider revising this.
  DWORD create_file_flags = FILE_FLAG_SEQUENTIAL_SCAN;

  if (asynchronous_mode)
    create_file_flags |= FILE_FLAG_OVERLAPPED;

  handle_ =
      CreateFile(path.c_str(), GENERIC_READ | SYNCHRONIZE,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL, OPEN_EXISTING, create_file_flags, NULL);
  if (handle_ == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    LOG(WARNING) << "Failed to open file: " << error;
    return MapErrorCode(error);
  }

  if (asynchronous_mode) {
    async_context_.reset(new AsyncContext(this));
    MessageLoopForIO::current()->RegisterIOHandler(handle_,
                                                   async_context_.get());
  }

  return OK;
}

bool FileInputStream::IsOpen() const {
  return handle_ != INVALID_HANDLE_VALUE;
}

int64 FileInputStream::Seek(Whence whence, int64 offset) {
  if (!IsOpen())
    return ERR_UNEXPECTED;
  DCHECK(!async_context_.get() || !async_context_->callback());

  LARGE_INTEGER distance, result;
  distance.QuadPart = offset;
  DWORD move_method = static_cast<DWORD>(whence);
  if (!SetFilePointerEx(handle_, distance, &result, move_method)) {
    DWORD error = GetLastError();
    LOG(WARNING) << "SetFilePointerEx failed: " << error;
    return MapErrorCode(error);
  }
  if (async_context_.get())
    SetOffset(async_context_->overlapped(), result);
  return result.QuadPart;
}

int64 FileInputStream::Available() {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  int64 cur_pos = Seek(FROM_CURRENT, 0);
  if (cur_pos < 0)
    return cur_pos;

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(handle_, &file_size)) {
    DWORD error = GetLastError();
    LOG(WARNING) << "GetFileSizeEx failed: " << error;
    return MapErrorCode(error);
  }

  return file_size.QuadPart - cur_pos;
}

int FileInputStream::Read(
    char* buf, int buf_len, CompletionCallback* callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  OVERLAPPED* overlapped = NULL;
  if (async_context_.get()) {
    DCHECK(!async_context_->callback());
    overlapped = async_context_->overlapped();
  }

  int rv;

  DWORD bytes_read;
  if (!ReadFile(handle_, buf, buf_len, &bytes_read, overlapped)) {
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
  } else {
    if (overlapped)
      IncrementOffset(overlapped, bytes_read);
    rv = static_cast<int>(bytes_read);
  }
  return rv;
}

}  // namespace net
