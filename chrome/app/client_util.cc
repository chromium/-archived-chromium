// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/client_util.h"
#include "chrome/installer/util/install_util.h"

#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

namespace {
bool ReadStrValueFromRegistry(const HKEY reg_key,
                              const wchar_t* const value_name,
                              wchar_t** value) {
  DWORD size = 0;
  if (::RegQueryValueEx(reg_key, value_name, NULL, NULL, NULL,
                        &size) != ERROR_SUCCESS) {
    return false;
  }

  *value = new wchar_t[1 + (size / sizeof(wchar_t))];
  if (::RegQueryValueEx(reg_key, value_name, NULL, NULL,
                        reinterpret_cast<BYTE*>(*value),
                        &size) != ERROR_SUCCESS) {
    delete[] *value;
    return false;
  }

  return true;
}
}

namespace client_util {
bool FileExists(const wchar_t* const file_path) {
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  return ::GetFileAttributesEx(file_path, GetFileExInfoStandard, &attrs) != 0;
}

bool GetChromiumVersion(const wchar_t* const exe_path,
                        const wchar_t* const reg_key_path,
                        wchar_t** version) {
  HKEY reg_root = InstallUtil::IsPerUserInstall(exe_path) ? HKEY_CURRENT_USER :
                                                            HKEY_LOCAL_MACHINE;
  HKEY reg_key;
  if (::RegOpenKeyEx(reg_root, reg_key_path, 0,
                     KEY_READ, &reg_key) != ERROR_SUCCESS) {
    return false;
  }

  std::wstring new_chrome_exe(exe_path);
  new_chrome_exe.append(installer_util::kChromeNewExe);
  if (FileExists(new_chrome_exe.c_str()) &&
      ReadStrValueFromRegistry(reg_key, google_update::kRegOldVersionField,
                               version)) {
    ::RegCloseKey(reg_key);
    return true;
  }

  bool ret = ReadStrValueFromRegistry(reg_key,
                                      google_update::kRegVersionField,
                                      version);
  ::RegCloseKey(reg_key);
  return ret;
}

std::wstring GetDLLPath(const std::wstring dll_name,
                        const std::wstring dll_path) {
  if (!dll_path.empty() && FileExists(dll_path.c_str()))
    return dll_path + L"\\" + dll_name;

  // This is not an official build. Find the dll using the default
  // path order in LoadLibrary.
  wchar_t path[MAX_PATH] = {0};
  wchar_t* file_part = NULL;
  DWORD result = ::SearchPath(NULL, dll_name.c_str(), NULL, MAX_PATH,
                              path, &file_part);
  if (result == 0 || result > MAX_PATH)
    return std::wstring();

  return path;
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

}  // namespace client_util
