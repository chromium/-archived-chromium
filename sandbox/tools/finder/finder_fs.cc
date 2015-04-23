// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
