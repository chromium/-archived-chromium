// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include "base/file_util.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/setup/setup.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/work_item.h"

namespace {
std::wstring AppendPath(const std::wstring parent_path,
                        const std::wstring path) {
  std::wstring new_path(parent_path);
  file_util::AppendToPath(&new_path, path);
  return new_path;
}


// This method adds work items to create (or update) Chrome uninstall entry in
// Control Panel->Add/Remove Programs list.
void AddUninstallShortcutWorkItems(HKEY reg_root,
                                   const std::wstring& exe_path,
                                   const std::wstring& install_path,
                                   const std::wstring& product_name,
                                   const std::wstring& new_version,
                                   WorkItemList* install_list) {
  std::wstring uninstall_cmd(L"\"");
  uninstall_cmd.append(installer::GetInstallerPathUnderChrome(install_path,
                                                              new_version));
  file_util::AppendToPath(&uninstall_cmd,
                          file_util::GetFilenameFromPath(exe_path));
  uninstall_cmd.append(L"\" --");
  uninstall_cmd.append(installer_util::switches::kUninstall);
  if (reg_root == HKEY_LOCAL_MACHINE) {
    uninstall_cmd.append(L" --");
    uninstall_cmd.append(installer_util::switches::kSystemLevel);
  }

  // Create DisplayName, UninstallString and InstallLocation keys
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring uninstall_reg = dist->GetUninstallRegPath();
  install_list->AddCreateRegKeyWorkItem(reg_root, uninstall_reg);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
      installer_util::kUninstallDisplayNameField, product_name, true);
  install_list->AddSetRegValueWorkItem(reg_root,
                                       uninstall_reg,
                                       installer_util::kUninstallStringField,
                                       uninstall_cmd, true);
  install_list->AddSetRegValueWorkItem(reg_root,
                                       uninstall_reg,
                                       L"InstallLocation", install_path, true);

  // DisplayIcon, NoModify and NoRepair
  std::wstring chrome_icon = AppendPath(install_path,
                                        installer_util::kChromeExe);
  ShellUtil::GetChromeIcon(chrome_icon);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"DisplayIcon", chrome_icon, true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"NoModify", 1, true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"NoRepair", 1, true);

  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"Publisher",
                                       dist->GetPublisherName(), true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"Version", new_version.c_str(), true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"DisplayVersion",
                                       new_version.c_str(), true);
  time_t rawtime = time(NULL);
  struct tm timeinfo = {0};
  localtime_s(&timeinfo, &rawtime);
  wchar_t buffer[9];
  if (wcsftime(buffer, 9, L"%Y%m%d", &timeinfo) == 8) {
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"InstallDate",
                                         buffer, false);
  }
}

void AddInstallerCopyTasks(const std::wstring& exe_path,
                           const std::wstring& archive_path,
                           const std::wstring& temp_path,
                           const std::wstring& install_path,
                           const std::wstring& new_version,
                           WorkItemList* install_list) {
  std::wstring installer_dir(installer::GetInstallerPathUnderChrome(
      install_path, new_version));
  install_list->AddCreateDirWorkItem(installer_dir);

  std::wstring exe_dst(installer_dir);
  std::wstring archive_dst(installer_dir);
  file_util::AppendToPath(&exe_dst,
      file_util::GetFilenameFromPath(exe_path));
  file_util::AppendToPath(&archive_dst,
      file_util::GetFilenameFromPath(archive_path));

  install_list->AddCopyTreeWorkItem(exe_path, exe_dst, temp_path,
                                    WorkItem::ALWAYS);
  install_list->AddMoveTreeWorkItem(archive_path, archive_dst, temp_path);
}


// This method tells if we are running on 64 bit platform so that we can copy
// one extra exe. If the API call to determine 64 bit fails, we play it safe
// and return true anyway so that the executable can be copied.
bool Is64bit() {
  typedef BOOL (WINAPI *WOW_FUNC)(HANDLE, PBOOL);
  BOOL is64 = FALSE;

  HANDLE handle = GetCurrentProcess();
  HMODULE module = GetModuleHandle(L"kernel32.dll");
  WOW_FUNC p = reinterpret_cast<WOW_FUNC>(GetProcAddress(module,
                                                         "IsWow64Process"));
  if ((p != NULL) && (!(p)(handle, &is64) || (is64 != FALSE))) {
    return true;
  }

  return false;
}

}

bool installer::InstallNewVersion(const std::wstring& exe_path,
                                  const std::wstring& archive_path,
                                  const std::wstring& src_path,
                                  const std::wstring& install_path,
                                  const std::wstring& temp_dir,
                                  const HKEY reg_root,
                                  const Version& new_version) {

  if (reg_root != HKEY_LOCAL_MACHINE && reg_root != HKEY_CURRENT_USER)
    return false;

  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(temp_dir);
  install_list->AddCreateDirWorkItem(install_path);

  // Move the version folder
  install_list->AddMoveTreeWorkItem(
      AppendPath(src_path, new_version.GetString()),
      AppendPath(install_path, new_version.GetString()),
      temp_dir);

  // Delete any new_chrome.exe if present (we will end up create a new one
  // if required) and then copy chrome.exe
  std::wstring new_chrome_exe = AppendPath(install_path,
                                           installer_util::kChromeNewExe);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey chrome_key(reg_root, dist->GetVersionKey().c_str(), KEY_READ);
  std::wstring current_version;
  if (file_util::PathExists(new_chrome_exe))
    chrome_key.ReadValue(google_update::kRegOldVersionField, &current_version);
  if (current_version.empty())
    chrome_key.ReadValue(google_update::kRegVersionField, &current_version);
  chrome_key.Close();

  install_list->AddDeleteTreeWorkItem(new_chrome_exe, std::wstring());
  install_list->AddCopyTreeWorkItem(
      AppendPath(src_path, installer_util::kChromeExe),
      AppendPath(install_path, installer_util::kChromeExe),
      temp_dir, WorkItem::NEW_NAME_IF_IN_USE, new_chrome_exe);

  // Extra executable for 64 bit systems.
  if (Is64bit()) {
    install_list->AddMoveTreeWorkItem(
        AppendPath(src_path, installer::kWowHelperExe),
        AppendPath(install_path, installer::kWowHelperExe),
        temp_dir);
  }

  // Copy the default Dictionaries only if the folder doesnt exist already
  install_list->AddCopyTreeWorkItem(
      AppendPath(src_path, installer::kDictionaries),
      AppendPath(install_path, installer::kDictionaries),
      temp_dir, WorkItem::IF_NOT_PRESENT);

  // Copy installer in install directory and
  // add shortcut in Control Panel->Add/Remove Programs.
  AddInstallerCopyTasks(exe_path, archive_path, temp_dir, install_path,
      new_version.GetString(), install_list.get());
  std::wstring product_name = dist->GetApplicationName();
  AddUninstallShortcutWorkItems(reg_root, exe_path, install_path,
      product_name, new_version.GetString(), install_list.get());

  // Delete any old_chrome.exe if present.
  install_list->AddDeleteTreeWorkItem(
      AppendPath(install_path, installer_util::kChromeOldExe), std::wstring());

  // Create Version key (if not already present) and set the new Chrome
  // version as last step.
  std::wstring version_key = dist->GetVersionKey();
  install_list->AddCreateRegKeyWorkItem(reg_root, version_key);
  install_list->AddSetRegValueWorkItem(reg_root, version_key,
                                       google_update::kRegNameField,
                                       product_name,
                                       true);    // overwrite name also
  install_list->AddSetRegValueWorkItem(reg_root, version_key,
                                       google_update::kRegVersionField,
                                       new_version.GetString(),
                                       true);    // overwrite version

  // Perform install operations.
  bool success = install_list->Do();

  // If the installation worked, handle the in-use update case:
  // * If new_chrome.exe exists, then currently Chrome was in use so save
  // current version in old version key.
  // * If new_chrome.exe doesnt exist, then delete old version key.
  if (success) {
    if (file_util::PathExists(new_chrome_exe)) {
      if (current_version.empty()) {
        LOG(ERROR) << "New chrome.exe exists but current version is empty!";
        success = false;
      } else {
        scoped_ptr<WorkItemList> inuse_list(WorkItem::CreateWorkItemList());
        inuse_list->AddSetRegValueWorkItem(reg_root,
                                           version_key,
                                           google_update::kRegOldVersionField,
                                           current_version.c_str(),
                                           true);
        std::wstring rename_cmd(installer::GetInstallerPathUnderChrome(
            install_path, new_version.GetString()));
        file_util::AppendToPath(&rename_cmd,
                                file_util::GetFilenameFromPath(exe_path));
        rename_cmd = L"\"" + rename_cmd +
                     L"\" --" + installer_util::switches::kRenameChromeExe;
        if (reg_root == HKEY_LOCAL_MACHINE) {
          rename_cmd = rename_cmd + L" --" +
                       installer_util::switches::kSystemLevel;
        }
        inuse_list->AddSetRegValueWorkItem(reg_root,
                                           version_key,
                                           google_update::kRegRenameCmdField,
                                           rename_cmd.c_str(),
                                           true);
        if (!inuse_list->Do()) {
          LOG(ERROR) << "Couldn't write old version/rename value to registry.";
          success = false;
          inuse_list->Rollback();
        }
      }
    } else {
      scoped_ptr<WorkItemList> inuse_list(WorkItem::CreateWorkItemList());
      inuse_list->AddDeleteRegValueWorkItem(reg_root, version_key,
                                            google_update::kRegOldVersionField,
                                            true);
      inuse_list->AddDeleteRegValueWorkItem(reg_root, version_key,
                                            google_update::kRegRenameCmdField,
                                            true);
      if (!inuse_list->Do()) {
        LOG(ERROR) << "Couldn't write old version/rename value to registry.";
        success = false;
        inuse_list->Rollback();
      }
    }
  }

  if (!success) {
    LOG(ERROR) << "Install failed, rolling back... ";
    install_list->Rollback();
    LOG(ERROR) << "Rollback complete. ";
  }
  return success;
}

