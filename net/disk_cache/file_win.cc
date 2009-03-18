// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/file.h"

#include "base/message_loop.h"
#include "base/singleton.h"
#include "net/disk_cache/disk_cache.h"

namespace {

// Structure used for asynchronous operations.
struct MyOverlapped {
  MyOverlapped(disk_cache::File* file, size_t offset,
               disk_cache::FileIOCallback* callback);
  ~MyOverlapped();
  OVERLAPPED* overlapped() {
    return &context_.overlapped;
  }

  MessageLoopForIO::IOContext context_;
  scoped_refptr<disk_cache::File> file_;
  disk_cache::FileIOCallback* callback_;
  const void* buffer_;
  bool delete_buffer_;  // Delete the user buffer at completion.
};

COMPILE_ASSERT(!offsetof(MyOverlapped, context_), starts_with_overlapped);

// Helper class to handle the IO completion notifications from the message loop.
class CompletionHandler : public MessageLoopForIO::IOHandler {
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD actual_bytes, DWORD error);
};

void CompletionHandler::OnIOCompleted(MessageLoopForIO::IOContext* context,
                                      DWORD actual_bytes, DWORD error) {
  MyOverlapped* data = reinterpret_cast<MyOverlapped*>(context);

  if (error) {
    DCHECK(!actual_bytes);
    actual_bytes = static_cast<DWORD>(-1);
    NOTREACHED();
  }

  if (data->callback_)
    data->callback_->OnFileIOComplete(static_cast<int>(actual_bytes));

  delete data;
}

MyOverlapped::MyOverlapped(disk_cache::File* file, size_t offset,
                           disk_cache::FileIOCallback* callback) {
  memset(this, 0, sizeof(*this));
  context_.handler = Singleton<CompletionHandler>::get();
  context_.overlapped.Offset = static_cast<DWORD>(offset);
  file_ = file;
  callback_ = callback;
}

MyOverlapped::~MyOverlapped() {
  if (delete_buffer_) {
    DCHECK(!callback_);
    // This whole thing could be updated to use IOBuffer, but PostWrite is not
    // used at the moment. TODO(rvargas): remove or update this code.
    delete[] reinterpret_cast<const char*>(buffer_);
  }
}

}  // namespace

namespace disk_cache {

// Used from WaitForPendingIO() when the cache is being destroyed.
MessageLoopForIO::IOHandler* GetFileIOHandler() {
  return Singleton<CompletionHandler>::get();
}

File::File(base::PlatformFile file)
    : init_(true), platform_file_(INVALID_HANDLE_VALUE),
      sync_platform_file_(file) {
}

bool File::Init(const std::wstring& name) {
  DCHECK(!init_);
  if (init_)
    return false;

  platform_file_ = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

  if (INVALID_HANDLE_VALUE == platform_file_)
    return false;

  MessageLoopForIO::current()->RegisterIOHandler(
      platform_file_, Singleton<CompletionHandler>::get());

  init_ = true;
  sync_platform_file_  = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                    OPEN_EXISTING, 0, NULL);

  if (INVALID_HANDLE_VALUE == sync_platform_file_)
    return false;

  return true;
}

File::~File() {
  if (!init_)
    return;

  if (INVALID_HANDLE_VALUE != platform_file_)
    CloseHandle(platform_file_);
  if (INVALID_HANDLE_VALUE != sync_platform_file_)
    CloseHandle(sync_platform_file_);
}

base::PlatformFile File::platform_file() const {
  DCHECK(init_);
  return (INVALID_HANDLE_VALUE == platform_file_) ? sync_platform_file_ :
                                                    platform_file_;
}

bool File::IsValid() const {
  if (!init_)
    return false;
  return (INVALID_HANDLE_VALUE != platform_file_ ||
          INVALID_HANDLE_VALUE != sync_platform_file_);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > LONG_MAX)
    return false;

  DWORD ret = SetFilePointer(sync_platform_file_, static_cast<LONG>(offset),
                             NULL, FILE_BEGIN);
  if (INVALID_SET_FILE_POINTER == ret)
    return false;

  DWORD actual;
  DWORD size = static_cast<DWORD>(buffer_len);
  if (!ReadFile(sync_platform_file_, buffer, size, &actual, NULL))
    return false;
  return actual == size;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  DWORD ret = SetFilePointer(sync_platform_file_, static_cast<LONG>(offset),
                             NULL, FILE_BEGIN);
  if (INVALID_SET_FILE_POINTER == ret)
    return false;

  DWORD actual;
  DWORD size = static_cast<DWORD>(buffer_len);
  if (!WriteFile(sync_platform_file_, buffer, size, &actual, NULL))
    return false;
  return actual == size;
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback)
    return Read(buffer, buffer_len, offset);

  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  MyOverlapped* data = new MyOverlapped(this, offset, callback);
  DWORD size = static_cast<DWORD>(buffer_len);

  DWORD actual;
  if (!ReadFile(platform_file_, buffer, size, &actual, data->overlapped())) {
    *completed = false;
    if (GetLastError() == ERROR_IO_PENDING)
      return true;
    delete data;
    return false;
  }

  // The operation completed already. We'll be called back anyway.
  *completed = (actual == size);
  DCHECK(actual == size);
  data->callback_ = NULL;
  data->file_ = NULL;  // There is no reason to hold on to this anymore.
  return *completed;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset,
                 FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback)
    return Write(buffer, buffer_len, offset);

  return AsyncWrite(buffer, buffer_len, offset, true, callback, completed);
}

bool File::PostWrite(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  return AsyncWrite(buffer, buffer_len, offset, false, NULL, NULL);
}

bool File::AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                      bool notify, FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  MyOverlapped* data = new MyOverlapped(this, offset, callback);
  bool dummy_completed;
  if (!callback) {
    DCHECK(!notify);
    data->delete_buffer_ = true;
    data->buffer_ = buffer;
    completed = &dummy_completed;
  }

  DWORD size = static_cast<DWORD>(buffer_len);

  DWORD actual;
  if (!WriteFile(platform_file_, buffer, size, &actual, data->overlapped())) {
    *completed = false;
    if (GetLastError() == ERROR_IO_PENDING)
      return true;
    delete data;
    return false;
  }

  // The operation completed already. We'll be called back anyway.
  *completed = (actual == size);
  DCHECK(actual == size);
  data->callback_ = NULL;
  data->file_ = NULL;  // There is no reason to hold on to this anymore.
  return *completed;
}

bool File::SetLength(size_t length) {
  DCHECK(init_);
  if (length > ULONG_MAX)
    return false;

  DWORD size = static_cast<DWORD>(length);
  HANDLE file = platform_file();
  if (INVALID_SET_FILE_POINTER == SetFilePointer(file, size, NULL, FILE_BEGIN))
    return false;

  return TRUE == SetEndOfFile(file);
}

size_t File::GetLength() {
  DCHECK(init_);
  LARGE_INTEGER size;
  HANDLE file = platform_file();
  if (!GetFileSizeEx(file, &size))
    return 0;
  if (size.HighPart)
    return ULONG_MAX;

  return static_cast<size_t>(size.LowPart);
}

}  // namespace disk_cache
