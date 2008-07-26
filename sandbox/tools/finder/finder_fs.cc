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

#include "sandbox/src/restricted_token.h"
#include "sandbox/src/restricted_token_utils.h"
#include "sandbox/tools/finder/finder.h"

DWORD Finder::ParseFileSystem(ATL::CString directory) {
  WIN32_FIND_DATA find_data;
  HANDLE find;

  //Search for items in the directory.
  ATL::CString name_to_search = directory + L"\\*";
  find = ::FindFirstFile(name_to_search, &find_data);
  if (INVALID_HANDLE_VALUE == find) {
    DWORD error = ::GetLastError();
    Output(FS_ERR, error, directory);
    filesystem_stats_[BROKEN]++;
    return error;
  }

  // parse all files or folders.
  do {
    if (_tcscmp(find_data.cFileName, L".") == 0 ||
        _tcscmp(find_data.cFileName, L"..") == 0)
      continue;

    ATL::CString complete_name = directory + L"\\" + find_data.cFileName;
    TestFileAccess(complete_name);

    // Call recursively the function if the path found is a directory.
    if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      ParseFileSystem(complete_name);
    }
  } while (::FindNextFile(find, &find_data) != 0);

  DWORD err_code = ::GetLastError();
  ::FindClose(find);

  if (ERROR_NO_MORE_FILES != err_code) {
    Output(FS_ERR, err_code, directory);
    filesystem_stats_[BROKEN]++;
    return err_code;
  }

  return ERROR_SUCCESS;
}

DWORD Finder::TestFileAccess(ATL::CString name) {
  Impersonater impersonate(token_handle_);

  filesystem_stats_[PARSE]++;

  HANDLE file;
  if (access_type_ & kTestForAll) {
    file = ::CreateFile(name.GetBuffer(),
                        GENERIC_ALL,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (file != INVALID_HANDLE_VALUE) {
      filesystem_stats_[ALL]++;
      Output(FS, L"R/W", name.GetBuffer());
      ::CloseHandle(file);
      return GENERIC_ALL;
    } else if (::GetLastError() != ERROR_ACCESS_DENIED) {
      Output(FS_ERR, GetLastError(), name);
      filesystem_stats_[BROKEN]++;
    }
  }

  if (access_type_ & kTestForWrite) {
    file = ::CreateFile(name.GetBuffer(),
                        GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (file != INVALID_HANDLE_VALUE) {
      filesystem_stats_[WRITE]++;
      Output(FS, L"W", name);
      ::CloseHandle(file);
      return GENERIC_WRITE;
    } else if (::GetLastError() != ERROR_ACCESS_DENIED) {
      Output(FS_ERR, ::GetLastError(), name);
      filesystem_stats_[BROKEN]++;
    }
  }

  if (access_type_ & kTestForRead) {
    file = ::CreateFile(name.GetBuffer(),
                        GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (file != INVALID_HANDLE_VALUE) {
      filesystem_stats_[READ]++;
      Output(FS, L"R", name);
      ::CloseHandle(file);
      return GENERIC_READ;
    } else if (::GetLastError() != ERROR_ACCESS_DENIED) {
      Output(FS_ERR, GetLastError(), name);
      filesystem_stats_[BROKEN]++;
    }
  }

  return 0;
}