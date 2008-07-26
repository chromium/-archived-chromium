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

bool  File::Init(const std::wstring name) {
  DCHECK(!init_);
  if (init_)
    return false;

  handle_ = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_FLAG_OVERLAPPED, NULL);

  if (INVALID_HANDLE_VALUE == handle_)
    return false;

  init_ = true;
  if (mixed_) {
    sync_handle_  = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == sync_handle_)
      return false;
  }

  return true;
}

File::~File() {
  if (!init_)
    return;

  CloseHandle(handle_);
  if (mixed_ && INVALID_HANDLE_VALUE != sync_handle_)
    CloseHandle(sync_handle_);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  if (!mixed_ || buffer_len > ULONG_MAX || offset > LONG_MAX)
    return false;

  DWORD ret = SetFilePointer(sync_handle_, static_cast<LONG>(offset), NULL,
                             FILE_BEGIN);
  if (INVALID_SET_FILE_POINTER == ret)
    return false;

  DWORD actual;
  DWORD size = static_cast<DWORD>(buffer_len);
  if (!ReadFile(sync_handle_, buffer, size, &actual, NULL))
    return false;
  return actual == size;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  if (!mixed_ || buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  DWORD ret = SetFilePointer(sync_handle_, static_cast<LONG>(offset), NULL,
                             FILE_BEGIN);
  if (INVALID_SET_FILE_POINTER == ret)
    return false;

  DWORD actual;
  DWORD size = static_cast<DWORD>(buffer_len);
  if (!WriteFile(sync_handle_, buffer, size, &actual, NULL))
    return false;
  return actual == size;
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
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

  if (!ReadFileEx(handle_, buffer, size, &data->overlapped, &IoCompletion)) {
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
  return AsyncWrite(buffer, buffer_len, offset, true, callback, completed);
}

bool File::PostWrite(const void* buffer, size_t buffer_len, size_t offset) {
  return AsyncWrite(buffer, buffer_len, offset, false, NULL, NULL);
}

bool File::AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                      bool notify, FileIOCallback* callback, bool* completed) {
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

  if (!WriteFileEx(handle_, buffer, size, &data->overlapped, &IoCompletion)) {
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
  if (length > ULONG_MAX)
    return false;

  DWORD size = static_cast<DWORD>(length);
  if (INVALID_SET_FILE_POINTER == SetFilePointer(handle_, size, NULL,
                                                 FILE_BEGIN))
    return false;

  return TRUE == SetEndOfFile(handle_);
}

size_t File::GetLength() {
  LARGE_INTEGER size;
  if (!GetFileSizeEx(handle_, &size))
    return 0;
  if (size.HighPart)
    return ULONG_MAX;

  return static_cast<size_t>(size.LowPart);
}

}  // namespace disk_cache
