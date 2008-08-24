// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/file.h"

#include "net/disk_cache/disk_cache.h"

namespace {

// This class implements FileIOCallback to perform IO operations
// when the callback parameter of the operation is NULL.
class SyncCallback: public disk_cache::FileIOCallback {
 public:
  SyncCallback() : called_(false) {}
  ~SyncCallback() {}

  virtual void OnFileIOComplete(int bytes_copied);
  void WaitForResult(int* bytes_copied);
 private:
  bool called_;
  int actual_;
};

void SyncCallback::OnFileIOComplete(int bytes_copied) {
  actual_ = bytes_copied;
  called_ = true;
}

// Waits for the IO operation to complete.
void SyncCallback::WaitForResult(int* bytes_copied) {
  for (;;) {
    SleepEx(INFINITE, TRUE);
    if (called_)
      break;
  }
  *bytes_copied = actual_;
}

// Structure used for asynchronous operations.
struct MyOverlapped {
  OVERLAPPED overlapped;
  disk_cache::File* file;
  disk_cache::FileIOCallback* callback;
  const void* buffer;
  DWORD actual_bytes;
  bool async;  // Invoke the callback form the completion.
  bool called;  // Completion received.
  bool delete_buffer;  // Delete the user buffer at completion.
};

COMPILE_ASSERT(!offsetof(MyOverlapped, overlapped), starts_with_overlapped);

}  // namespace

namespace disk_cache {

// SyncCallback to be invoked as an APC when the asynchronous operation
// completes.
void CALLBACK IoCompletion(DWORD error, DWORD actual_bytes,
                           OVERLAPPED* overlapped) {
  MyOverlapped* data = reinterpret_cast<MyOverlapped*>(overlapped);

  if (error) {
    DCHECK(!actual_bytes);
    actual_bytes = static_cast<DWORD>(-1);
    NOTREACHED();
  }

  if (data->delete_buffer) {
    DCHECK(!data->callback);
    data->file->Release();
    delete data->buffer;
    delete data;
    return;
  }

  if (data->async) {
    data->callback->OnFileIOComplete(static_cast<int>(actual_bytes));
    data->file->Release();
    delete data;
  } else {
    // Somebody is waiting for this so don't delete data and instead notify
    // that we were called.
    data->actual_bytes = actual_bytes;
    data->file->Release();
    data->called = true;
  }
}

File::File(OSFile file)
    : init_(true), mixed_(true), os_file_(INVALID_HANDLE_VALUE),
      sync_os_file_(file) {
}

bool File::Init(const std::wstring& name) {
  DCHECK(!init_);
  if (init_)
    return false;

  os_file_ = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED, NULL);

  if (INVALID_HANDLE_VALUE == os_file_)
    return false;

  init_ = true;
  if (mixed_) {
    sync_os_file_  = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == sync_os_file_)
      return false;
  } else {
    sync_os_file_ = INVALID_HANDLE_VALUE;
  }

  return true;
}

File::~File() {
  if (!init_)
    return;

  if (INVALID_HANDLE_VALUE != os_file_)
    CloseHandle(os_file_);
  if (mixed_ && INVALID_HANDLE_VALUE != sync_os_file_)
    CloseHandle(sync_os_file_);
}

OSFile File::os_file() const {
  DCHECK(init_);
  return (INVALID_HANDLE_VALUE == os_file_) ? sync_os_file_ : os_file_;
}

bool File::IsValid() const {
  if (!init_)
    return false;
  return (INVALID_HANDLE_VALUE != os_file_ ||
          INVALID_HANDLE_VALUE != sync_os_file_);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (!mixed_ || buffer_len > ULONG_MAX || offset > LONG_MAX)
    return false;

  DWORD ret = SetFilePointer(sync_os_file_, static_cast<LONG>(offset), NULL,
                             FILE_BEGIN);
  if (INVALID_SET_FILE_POINTER == ret)
    return false;

  DWORD actual;
  DWORD size = static_cast<DWORD>(buffer_len);
  if (!ReadFile(sync_os_file_, buffer, size, &actual, NULL))
    return false;
  return actual == size;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (!mixed_ || buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  DWORD ret = SetFilePointer(sync_os_file_, static_cast<LONG>(offset), NULL,
                             FILE_BEGIN);
  if (INVALID_SET_FILE_POINTER == ret)
    return false;

  DWORD actual;
  DWORD size = static_cast<DWORD>(buffer_len);
  if (!WriteFile(sync_os_file_, buffer, size, &actual, NULL))
    return false;
  return actual == size;
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  MyOverlapped* data = new MyOverlapped;
  memset(data, 0, sizeof(*data));

  SyncCallback local_callback;
  data->overlapped.Offset = static_cast<DWORD>(offset);
  data->callback = callback ? callback : &local_callback;
  data->file = this;

  DWORD size = static_cast<DWORD>(buffer_len);
  AddRef();

  if (!ReadFileEx(os_file_, buffer, size, &data->overlapped, &IoCompletion)) {
    Release();
    delete data;
    return false;
  }

  if (callback) {
    *completed = false;
    // Let's check if the operation is already finished.
    SleepEx(0, TRUE);
    if (data->called) {
      *completed = (data->actual_bytes == size);
      DCHECK(data->actual_bytes == size);
      delete data;
      return *completed;
    }
    data->async = true;
  } else {
    // Invoke the callback and perform cleanup on the APC.
    data->async = true;
    int bytes_copied;
    local_callback.WaitForResult(&bytes_copied);
    if (static_cast<int>(buffer_len) != bytes_copied) {
      NOTREACHED();
      return false;
    }
  }

  return true;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset,
                 FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
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

  MyOverlapped* data = new MyOverlapped;
  memset(data, 0, sizeof(*data));

  SyncCallback local_callback;
  data->overlapped.Offset = static_cast<DWORD>(offset);
  data->callback = callback ? callback : &local_callback;
  data->file = this;
  if (!callback && !notify) {
    data->delete_buffer = true;
    data->callback = NULL;
    data->buffer = buffer;
  }

  DWORD size = static_cast<DWORD>(buffer_len);
  AddRef();

  if (!WriteFileEx(os_file_, buffer, size, &data->overlapped, &IoCompletion)) {
    Release();
    delete data;
    return false;
  }

  if (callback) {
    *completed = false;
    SleepEx(0, TRUE);
    if (data->called) {
      *completed = (data->actual_bytes == size);
      DCHECK(data->actual_bytes == size);
      delete data;
      return *completed;
    }
    data->async = true;
  } else if (notify) {
    data->async = true;
    int bytes_copied;
    local_callback.WaitForResult(&bytes_copied);
    if (static_cast<int>(buffer_len) != bytes_copied) {
      NOTREACHED();
      return false;
    }
  }

  return true;
}

bool File::SetLength(size_t length) {
  DCHECK(init_);
  if (length > ULONG_MAX)
    return false;

  DWORD size = static_cast<DWORD>(length);
  HANDLE file = os_file();
  if (INVALID_SET_FILE_POINTER == SetFilePointer(file, size, NULL, FILE_BEGIN))
    return false;

  return TRUE == SetEndOfFile(file);
}

size_t File::GetLength() {
  DCHECK(init_);
  LARGE_INTEGER size;
  HANDLE file = os_file();
  if (!GetFileSizeEx(file, &size))
    return 0;
  if (size.HighPart)
    return ULONG_MAX;

  return static_cast<size_t>(size.LowPart);
}

}  // namespace disk_cache

