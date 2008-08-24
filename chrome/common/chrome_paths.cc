// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "chrome/common/chrome_paths.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"

namespace chrome {

bool GetUserDirectory(int directory_type, std::wstring* result) {
  wchar_t path_buf[MAX_PATH];
  if (FAILED(SHGetFolderPath(NULL, directory_type, NULL,
                             SHGFP_TYPE_CURRENT, path_buf)))
    return false;
  result->assign(path_buf);
  return true;
}

// Gets the default user data directory, regardless of whether
// DIR_USER_DATA has been overridden by a command-line option.
bool GetDefaultUserDataDirectory(std::wstring* result) {
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  file_util::AppendToPath(result, L"Google");
  file_util::AppendToPath(result, chrome::kBrowserAppName);
  file_util::AppendToPath(result, chrome::kUserDataDirname);
  return true;
}

bool GetGearsPluginPathFromCommandLine(std::wstring *path) {
#ifndef NDEBUG
  // for debugging, support a cmd line based override
  CommandLine command_line;
  *path = command_line.GetSwitchValue(switches::kGearsPluginPathOverride);
  return !path->empty();
#else
  return false;
#endif
}

bool PathProvider(int key, std::wstring* result) {
  // Some keys are just aliases...
  switch (key) {
    case chrome::DIR_APP:
      return PathService::Get(base::DIR_MODULE, result);
    case chrome::DIR_LOGS:
#ifndef NDEBUG
      return PathService::Get(chrome::DIR_USER_DATA, result);
#else
      return PathService::Get(base::DIR_EXE, result);
#endif
    case chrome::FILE_RESOURCE_MODULE:
      return PathService::Get(base::FILE_MODULE, result);
  }

  // We need to go compute the value. It would be nice to support paths with
  // names longer than MAX_PATH, but the system functions don't seem to be
  // designed for it either, with the exception of GetTempPath (but other
  // things will surely break if the temp path is too long, so we don't bother
  // handling it.
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;

  // Assume that we will need to create the directory if it does not already
  // exist.  This flag can be set to true to prevent checking.
  bool exists = false;

  std::wstring cur;
  switch (key) {
    case chrome::DIR_USER_DATA:
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
      break;
    case chrome::DIR_USER_DOCUMENTS:
      if (!GetUserDirectory(CSIDL_MYDOCUMENTS, &cur))
        return false;
      break;
    case chrome::DIR_CRASH_DUMPS:
      // The crash reports are always stored relative to the default user data
      // directory.  This avoids the problem of having to re-initialize the
      // exception handler after parsing command line options, which may
      // override the location of the app's profile directory.
      if (!GetDefaultUserDataDirectory(&cur))
        return false;
      file_util::AppendToPath(&cur, L"Crash Reports");
      break;
    case chrome::DIR_DEFAULT_DOWNLOAD:
      if (FAILED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
                                 SHGFP_TYPE_CURRENT, system_buffer)))
        return false;
      cur = system_buffer;
      exists = true;
      break;
    case chrome::DIR_RESOURCES:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::AppendToPath(&cur, L"resources");
      break;
    case chrome::DIR_INSPECTOR:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::AppendToPath(&cur, L"Resources");
      file_util::AppendToPath(&cur, L"Inspector");
      exists = true;
      break;
    case chrome::DIR_THEMES:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::AppendToPath(&cur, L"themes");
      break;
    case chrome::DIR_LOCALES:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::AppendToPath(&cur, L"locales");
      break;
    case chrome::DIR_APP_DICTIONARIES:
      if (!PathService::Get(base::DIR_EXE, &cur))
        return false;
      file_util::AppendToPath(&cur, L"Dictionaries");
      break;
    case chrome::FILE_LOCAL_STATE:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      file_util::AppendToPath(&cur, chrome::kLocalStateFilename);
      exists = true;  // don't trigger directory creation code
      break;
    case chrome::FILE_RECORDED_SCRIPT:
      if (!PathService::Get(chrome::DIR_USER_DATA, &cur))
        return false;
      file_util::AppendToPath(&cur, L"script.log");
      exists = true;
      break;
    case chrome::FILE_GEARS_PLUGIN:
      if (!GetGearsPluginPathFromCommandLine(&cur)) {
        if (!PathService::Get(base::DIR_EXE, &cur))
          return false;
        file_util::AppendToPath(&cur, L"plugins");
        file_util::AppendToPath(&cur, L"gears");
        file_util::AppendToPath(&cur, L"gears.dll");
      }
      exists = true;
      break;
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case chrome::DIR_TEST_DATA:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::UpOneDirectory(&cur);
      file_util::AppendToPath(&cur, L"test");
      file_util::AppendToPath(&cur, L"data");
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      exists = true;
      break;
    case chrome::DIR_TEST_TOOLS:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::UpOneDirectory(&cur);
      file_util::AppendToPath(&cur, L"tools");
      file_util::AppendToPath(&cur, L"test");
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      exists = true;
      break;
    case chrome::FILE_PYTHON_RUNTIME:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::UpOneDirectory(&cur);  // chrome
      file_util::UpOneDirectory(&cur);
      file_util::AppendToPath(&cur, L"third_party");
      file_util::AppendToPath(&cur, L"python_24");
      file_util::AppendToPath(&cur, L"python.exe");
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      exists = true;
      break;
    case chrome::FILE_TEST_SERVER:
      if (!PathService::Get(chrome::DIR_APP, &cur))
        return false;
      file_util::UpOneDirectory(&cur);
      file_util::AppendToPath(&cur, L"tools");
      file_util::AppendToPath(&cur, L"test");
      file_util::AppendToPath(&cur, L"testserver");
      file_util::AppendToPath(&cur, L"testserver.py");
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;
      exists = true;
      break;
    default:
      return false;
  }

  if (!exists && !file_util::PathExists(cur) && !file_util::CreateDirectory(cur))
    return false;

  result->swap(cur);
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace chrome

