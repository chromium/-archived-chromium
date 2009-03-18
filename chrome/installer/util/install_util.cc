// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See the corresponding header file for description of the functions in this
// file.

#include "chrome/installer/util/install_util.h"

#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win_util.h"

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/work_item_list.h"

bool InstallUtil::ExecuteExeAsAdmin(const std::wstring& exe,
                                    const std::wstring& params,
                                    DWORD* exit_code) {
  SHELLEXECUTEINFO info = {0};
  info.cbSize = sizeof(SHELLEXECUTEINFO);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"runas";
  info.lpFile = exe.c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOW;
  if (::ShellExecuteEx(&info) == FALSE)
    return false;

  ::WaitForSingleObject(info.hProcess, INFINITE);
  DWORD ret_val = 0;
  if (!::GetExitCodeProcess(info.hProcess, &ret_val))
    return false;

  if (exit_code)
    *exit_code = ret_val;
  return true;
}

std::wstring InstallUtil::GetChromeUninstallCmd(bool system_install) {
  HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey key(root, dist->GetUninstallRegPath().c_str());
  std::wstring uninstall_cmd;
  key.ReadValue(installer_util::kUninstallStringField, &uninstall_cmd);
  return uninstall_cmd;
}

installer::Version* InstallUtil::GetChromeVersion(bool system_install) {
  RegKey key;
  std::wstring version_str;

  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!key.Open(reg_root, dist->GetVersionKey().c_str(), KEY_READ) ||
      !key.ReadValue(google_update::kRegVersionField, &version_str)) {
    LOG(INFO) << "No existing Chrome install found.";
    key.Close();
    return NULL;
  }
  key.Close();
  LOG(INFO) << "Existing Chrome version found " << version_str;
  return installer::Version::GetVersionFromString(version_str);
}

bool InstallUtil::IsOSSupported() {
  int major, minor;
  win_util::WinVersion version = win_util::GetWinVersion();
  win_util::GetServicePackLevel(&major, &minor);

  // We do not support Win2K or older, or XP without service pack 1.
  LOG(INFO) << "Windows Version: " << version
            << ", Service Pack: " << major << "." << minor;
  if ((version > win_util::WINVERSION_XP) ||
      (version == win_util::WINVERSION_XP && major >= 1)) {
    return true;
  }
  return false;
}

void InstallUtil::WriteInstallerResult(bool system_install,
                                       installer_util::InstallStatus status,
                                       int string_resource_id,
                                       const std::wstring* const launch_cmd) {
  HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring key = dist->GetStateKey();
  int installer_result = (dist->GetInstallReturnCode(status) == 0) ? 0 : 1;
  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddCreateRegKeyWorkItem(root, key);
  install_list->AddSetRegValueWorkItem(root, key, L"InstallerResult",
                                       installer_result, true);
  install_list->AddSetRegValueWorkItem(root, key, L"InstallerError",
                                       status, true);
  if (string_resource_id != 0) {
    std::wstring msg = installer_util::GetLocalizedString(string_resource_id);
    install_list->AddSetRegValueWorkItem(root, key, L"InstallerResultUIString",
                                         msg, true);
  }
  // TODO(rahulk) verify that the absence of InstallerSuccessLaunchCmdLine
  // for non-system installs does not break the gcapi.dll.
  if ((launch_cmd != NULL) && system_install) {
    install_list->AddSetRegValueWorkItem(root, key,
                                         L"InstallerSuccessLaunchCmdLine",
                                         *launch_cmd, true);
  }
  if (!install_list->Do())
    LOG(ERROR) << "Failed to record installer error information in registry.";
}

bool InstallUtil::IsPerUserInstall(const wchar_t* const exe_path) {
  wchar_t program_files_path[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                SHGFP_TYPE_CURRENT, program_files_path))) {
    return !StartsWith(exe_path, program_files_path, false);
  } else {
    NOTREACHED();
  }
  return true;
}

bool InstallUtil::BuildDLLRegistrationList(const std::wstring& install_path,
                                           const wchar_t** const dll_names,
                                           int dll_names_count,
                                           bool do_register,
                                           WorkItemList* registration_list) {
  DCHECK(NULL != registration_list);
  bool success = true;
  for (int i = 0; i < dll_names_count; i++) {
    std::wstring dll_file_path(install_path);
    file_util::AppendToPath(&dll_file_path, dll_names[i]);
    success = registration_list->AddSelfRegWorkItem(dll_file_path,
        do_register) && success;
  }
  return (dll_names_count > 0) && success;
}
