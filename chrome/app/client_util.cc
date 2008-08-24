// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/client_util.h"

#include <shlobj.h>

namespace client_util {
const wchar_t kProductVersionKey[] = L"pv";

bool FileExists(const wchar_t* const file_path) {
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  return ::GetFileAttributesEx(file_path, GetFileExInfoStandard, &attrs) != 0;
}

bool GetChromiumVersion(const wchar_t* const exe_path,
                        const wchar_t* const reg_key_path,
                        wchar_t** version) {
  HKEY reg_root = IsUserModeInstall(exe_path) ? HKEY_CURRENT_USER :
                                                HKEY_LOCAL_MACHINE;
  HKEY reg_key;
  if (::RegOpenKeyEx(reg_root, reg_key_path, 0,
                     KEY_READ, &reg_key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD size = 0;
  bool ret = false;
  if (::RegQueryValueEx(reg_key, client_util::kProductVersionKey, NULL, NULL,
                        NULL, &size) == ERROR_SUCCESS) {
    *version = new wchar_t[1 + (size / sizeof(wchar_t))];
    if (::RegQueryValueEx(reg_key, client_util::kProductVersionKey,
                          NULL, NULL, reinterpret_cast<BYTE*>(*version),
                          &size) == ERROR_SUCCESS) {
      ret = true;
    } else {
      delete[] *version;
    }
  }
  ::RegCloseKey(reg_key);
  return ret;
}

void GetExecutablePath(wchar_t* exe_path) {
  DWORD len = ::GetModuleFileName(NULL, exe_path, MAX_PATH);
  wchar_t* tmp = exe_path + len - 1;
  while (tmp >= exe_path && *tmp != L'\\')
    tmp--;
  if (tmp > exe_path) {
    tmp++;
    *tmp = 0;
  }
}

bool IsUserModeInstall(const wchar_t* const exe_path) {
  wchar_t buffer[MAX_PATH] = {0};
  if (!FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                              SHGFP_TYPE_CURRENT, buffer))) {
    if (exe_path == wcsstr(exe_path, buffer)) {
      return false;
    }
  }
  return true;
}
}  // namespace client_util

