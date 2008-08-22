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

#include "net/disk_cache/cache_util.h"

#include <windows.h>

#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/file_util.h"

namespace {

// Deletes all the files on path that match search_name pattern.
void DeleteFiles(const wchar_t* path, const wchar_t* search_name) {
  std::wstring name(path);
  file_util::AppendToPath(&name, search_name);

  WIN32_FIND_DATA data;
  ScopedFindFileHandle handle(FindFirstFile(name.c_str(), &data));
  if (!handle.IsValid())
    return;

  std::wstring adjusted_path(path);
  adjusted_path += L'\\';
  do {
    if (data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY ||
        data.dwFileAttributes == FILE_ATTRIBUTE_REPARSE_POINT)
      continue;
    std::wstring current(adjusted_path);
    current += data.cFileName;
    DeleteFile(current.c_str());
  } while (FindNextFile(handle, &data));
}

}  // namespace

namespace disk_cache {

int64 GetFreeDiskSpace(const std::wstring& path) {
  ULARGE_INTEGER available, total, free;
  if (!GetDiskFreeSpaceExW(path.c_str(), &available, &total, &free)) {
    return -1;
  }
  int64 rv = static_cast<int64>(available.QuadPart);
  if (rv < 0)
    rv = kint64max;
  return rv;
}

int64 GetSystemMemory() {
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(memory_info);
  if (!GlobalMemoryStatusEx(&memory_info)) {
    return -1;
  }

  int64 rv = static_cast<int64>(memory_info.ullTotalPhys);
  if (rv < 0)
    rv = kint64max;
  return rv;
}

bool MoveCache(const std::wstring& from_path, const std::wstring& to_path) {
  // I don't want to use the shell version of move because if something goes
  // wrong, that version will attempt to move file by file and fail at the end.
  if (!MoveFileEx(from_path.c_str(), to_path.c_str(), 0)) {
    LOG(ERROR) << "Unable to move the cache: " << GetLastError();
    return false;
  }
  return true;
}

void DeleteCache(const std::wstring& path, bool remove_folder) {
  DeleteFiles(path.c_str(), L"*");
  if (remove_folder)
    RemoveDirectory(path.c_str());
}

bool DeleteCacheFile(const std::wstring& name) {
  // We do a simple delete, without ever falling back to SHFileOperation, as the
  // version from base does.
  return DeleteFile(name.c_str()) ? true : false;
}

void WaitForPendingIO(int* num_pending_io) {
  while (*num_pending_io) {
    // Asynchronous IO operations may be in flight and the completion may end
    // up calling us back so let's wait for them (we need an alertable wait).
    // The idea is to let other threads do usefull work and at the same time
    // allow more than one IO to finish... 20 mS later, we process all queued
    // APCs and see if we have to repeat the wait.
    Sleep(20);
    SleepEx(0, TRUE);
  }
}

}  // namespace disk_cache
