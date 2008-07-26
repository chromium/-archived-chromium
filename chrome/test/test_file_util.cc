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

#include <vector>
#include <windows.h>

#include "chrome/test/test_file_util.h"

#include "base/file_util.h"
#include "chrome/common/win_util.h"

namespace file_util {

bool EvictFileFromSystemCache(const wchar_t* file) {
  // Request exclusive access to the file and overwrite it with no buffering.
  win_util::ScopedHandle hfile(
      CreateFile(file, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                 OPEN_EXISTING, FILE_FLAG_NO_BUFFERING,
                 NULL));
  if (!hfile)
    return false;

  // Execute in chunks. It could be optimized. We want to do few of these since
  // these opterations will be slow without the cache.
  char buffer[4096];
  int total_bytes = 0;
  DWORD bytes_read;
  for (;;) {
    bytes_read = 0;
    ReadFile(hfile, buffer, sizeof(buffer), &bytes_read, NULL);
    if (bytes_read == 0)
      break;

    SetFilePointer(hfile, total_bytes, 0, FILE_BEGIN);
    if (!WriteFile(hfile, buffer, bytes_read, &bytes_read, NULL))
      return false;
    total_bytes += bytes_read;
  }
  return true;
}

// Like CopyFileNoCache but recursively copies all files and subdirectories
// in the given input directory to the output directory.
bool CopyRecursiveDirNoCache(const std::wstring& source_dir,
                             const std::wstring& dest_dir) {
  // Try to create the directory if it doesn't already exist.
  if (!CreateDirectory(dest_dir)) {
    if (GetLastError() != ERROR_ALREADY_EXISTS)
      return false;
  }

  std::vector<std::wstring> files_copied;

  std::wstring src(source_dir);
  file_util::AppendToPath(&src, L"*");

  WIN32_FIND_DATA fd;
  HANDLE fh = FindFirstFile(src.c_str(), &fd);
  if (fh == INVALID_HANDLE_VALUE)
    return false;

  do {
    std::wstring cur_file(fd.cFileName);
    if (cur_file == L"." || cur_file == L"..")
      continue;  // Skip these special entries.

    std::wstring cur_source_path(source_dir);
    file_util::AppendToPath(&cur_source_path, cur_file);

    std::wstring cur_dest_path(dest_dir);
    file_util::AppendToPath(&cur_dest_path, cur_file);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // Recursively copy a subdirectory. We stripped "." and ".." already.
      if (!CopyRecursiveDirNoCache(cur_source_path, cur_dest_path)) {
        FindClose(fh);
        return false;
      }
    } else {
      // Copy the file.
      if (!::CopyFile(cur_source_path.c_str(), cur_dest_path.c_str(), false)) {
        FindClose(fh);
        return false;
      }

      // We don't check for errors from this function, often, we are copying
      // files that are in the repository, and they will have read-only set.
      // This will prevent us from evicting from the cache, but these don't
      // matter anyway.
      EvictFileFromSystemCache(cur_dest_path.c_str());
    }
  } while (FindNextFile(fh, &fd));

  FindClose(fh);
  return true;
}

}  // namespace file_util
