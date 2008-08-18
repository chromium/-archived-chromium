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

#include <windows.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"
#include "chrome/installer/util/work_item_list.h"

std::wstring installer::GetChromeInstallPath(bool system_install) {
  std::wstring install_path;
  if (system_install) {
    PathService::Get(base::DIR_PROGRAM_FILES, &install_path);
  } else {
    PathService::Get(base::DIR_LOCAL_APP_DATA, &install_path);
  }

  if (!install_path.empty()) {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    file_util::AppendToPath(&install_path, dist->GetInstallSubDir());
    file_util::AppendToPath(&install_path,
                            std::wstring(installer_util::kInstallBinaryDir));
  }

  return install_path;
}

bool installer::LaunchChrome(bool system_install) {
  std::wstring chrome_exe(L"\"");
  chrome_exe.append(installer::GetChromeInstallPath(system_install));
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
  chrome_exe.append(L"\"");
  return process_util::LaunchApp(chrome_exe, false, false, NULL);
}

bool installer::LaunchChromeAndWaitForResult(bool system_install,
                                             const std::wstring& options,
                                             int32 timeout_ms,
                                             int32* exit_code,
                                             bool* is_timeout) {
  std::wstring chrome_exe(installer::GetChromeInstallPath(system_install));
  if (chrome_exe.empty())
    return false;
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);

  std::wstring command_line(chrome_exe);
  command_line.append(options);
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcessW(chrome_exe.c_str(),
                        const_cast<wchar_t*>(command_line.c_str()),
                        NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                        &si, &pi)) {
    return false;
  }

  DWORD wr = ::WaitForSingleObject(pi.hProcess, timeout_ms);
  if (WAIT_TIMEOUT == wr) {
    if (is_timeout)
      *is_timeout = true;
  } else {  // WAIT_OBJECT_0
    if (is_timeout)
      *is_timeout = false;
    if (exit_code) {
      ::GetExitCodeProcess(pi.hProcess, reinterpret_cast<DWORD*>(exit_code));
    }
  }

  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);
  return true;
}

void installer::RemoveOldVersionDirs(const std::wstring& chrome_path,
                                     const std::wstring& latest_version_str) {
  std::wstring search_path(chrome_path);
  file_util::AppendToPath(&search_path, L"*");

  WIN32_FIND_DATA find_file_data;
  HANDLE file_handle = FindFirstFile(search_path.c_str(), &find_file_data);
  if (file_handle == INVALID_HANDLE_VALUE)
    return;

  BOOL ret = TRUE;
  scoped_ptr<installer::Version> version;
  scoped_ptr<installer::Version> latest_version(
      installer::Version::GetVersionFromString(latest_version_str));

  // We try to delete all directories whose versions are lower than
  // latest_version.
  while (ret) {
    if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      LOG(INFO) << "directory found: " << find_file_data.cFileName;
      version.reset(
          installer::Version::GetVersionFromString(find_file_data.cFileName));
      if (version.get() && latest_version->IsHigherThan(version.get())) {
        std::wstring remove_dir(chrome_path);
        file_util::AppendToPath(&remove_dir, find_file_data.cFileName);
        std::wstring chrome_dll_path(remove_dir);
        file_util::AppendToPath(&chrome_dll_path, installer_util::kChromeDll);
        LOG(INFO) << "deleting directory " << remove_dir;
        scoped_ptr<DeleteTreeWorkItem> item;
        item.reset(WorkItem::CreateDeleteTreeWorkItem(remove_dir,
                                                      chrome_dll_path));
        item->Do();
      }
    }
    ret = FindNextFile(file_handle, &find_file_data);
  }

  FindClose(file_handle);
}
