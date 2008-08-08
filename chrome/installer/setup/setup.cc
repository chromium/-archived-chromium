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

#include <shlobj.h>

#include "chrome/installer/setup/setup.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"
#include "chrome/installer/util/work_item_list.h"

#include "setup_strings.h"

namespace {

void AddChromeToMediaPlayerList() {
  std::wstring reg_path(installer::kMediaPlayerRegPath);
  // registry paths can also be appended like file system path
  file_util::AppendToPath(&reg_path, installer_util::kChromeExe);
  LOG(INFO) << "Adding Chrome to Media player list at " << reg_path;
  scoped_ptr<WorkItem> work_item(WorkItem::CreateCreateRegKeyWorkItem(
      HKEY_LOCAL_MACHINE, reg_path));

  // if the operation fails we log the error but still continue
  if (!work_item.get()->Do())
    LOG(ERROR) << "Couldn't add Chrome to media player inclusion list.";

}

// If we ever rename Chrome shortcuts under Windows Start menu, this
// function deletes any of the old Chrome shortcuts if they exist. This
// function will probably contain hard coded names of old shortcuts as they
// will not longer be used anywhere else.
// Method returns true if it finds and deletes successfully any old shortcut,
// in all other cases it returns false.
bool DeleteOldShortcuts(const std::wstring shortcut_path) {
  // Check for the existence of shortcuts when they were still created under
  // Start->Programs->Chrome (as opposed to Start->Programs->Google Chrome).
  std::wstring shortcut_folder(shortcut_path);
  file_util::AppendToPath(&shortcut_folder, L"Chrome");
  if (file_util::PathExists(shortcut_folder)) {
    LOG(INFO) << "Old shortcut path " << shortcut_folder << " exists.";
    return file_util::Delete(shortcut_folder, true);
  }
  return false;
}

// Update shortcuts that are created by chrome.exe during first run, but
// we take care of updating them in case the location of chrome.exe changes.
void UpdateChromeExeShortcuts(const std::wstring& chrome_exe) {
  std::wstring desktop_shortcut, ql_shortcut, shortcut_name;
  if (!ShellUtil::GetQuickLaunchPath(&ql_shortcut) ||
      !ShellUtil::GetDesktopPath(&desktop_shortcut) ||
      !ShellUtil::GetChromeShortcutName(&shortcut_name))
    return;
  // Migrate the old shortcuts from Chrome.lnk to the localized name.
  std::wstring old_ql_shortcut = ql_shortcut;
  file_util::AppendToPath(&old_ql_shortcut, L"Chrome.lnk");
  file_util::AppendToPath(&ql_shortcut, shortcut_name);
  std::wstring old_desktop_shortcut = desktop_shortcut;
  file_util::AppendToPath(&old_desktop_shortcut, L"Chrome.lnk");
  file_util::AppendToPath(&desktop_shortcut, shortcut_name);

  if (file_util::Move(old_ql_shortcut, ql_shortcut)) {
    // Notify the Windows Shell that we renamed the file so it can remove the
    // old icon.  It's safe to not cehck for MAX_PATH because file_util::Move
    // does the check for us.
    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATH, old_ql_shortcut.c_str(),
                   ql_shortcut.c_str());
  }
  if (file_util::Move(old_desktop_shortcut, desktop_shortcut)) {
    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATH, old_desktop_shortcut.c_str(),
                   desktop_shortcut.c_str());
  }

  // Go ahead and update the shortcuts if they exist.
  ShellUtil::UpdateChromeShortcut(chrome_exe, ql_shortcut, false);
  ShellUtil::UpdateChromeShortcut(chrome_exe, desktop_shortcut, false);
}

// This method creates Chrome shortcuts in Start->Programs for all users or
// only for current user depending on whether it is system wide install or
// user only install.
//
// If first_install is true, it creates shortcuts for launching and
// uninstalling chrome.
// If first_install is false, the function only updates the shortcut for
// uninstalling chrome. According to
// http://blogs.msdn.com/oldnewthing/archive/2005/11/24/496690.aspx,
// updating uninstall shortcut should not trigger Windows "new application
// installed" notification.
//
// If the shortcuts do not exist, the function does not recreate them during
// update.
bool CreateOrUpdateChromeShortcuts(const std::wstring& exe_path,
                                   bool system_install,
                                   installer_util::InstallStatus install_status,
                                   const std::wstring& install_path,
                                   const std::wstring& new_version) {
  std::wstring shortcut_path;
  int dir_enum = (system_install) ? base::DIR_COMMON_START_MENU :
                                    base::DIR_START_MENU;
  if (!PathService::Get(dir_enum, &shortcut_path)) {
    LOG(ERROR) << "Failed to get location for shortcut.";
    return false;
  }

  // Check for existence of old shortcuts
  bool old_shortcuts_existed = DeleteOldShortcuts(shortcut_path);

  // The location of Start->Programs->Google Chrome folder
  const std::wstring& product_name =
      installer_util::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
  file_util::AppendToPath(&shortcut_path, product_name);

  // Create/update Chrome link (points to chrome.exe) & Uninstall Chrome link
  // (which points to setup.exe) under this folder only if:
  // - This is a new install or install repair
  // OR
  // - The shortcut already exists in case of updates (user may have deleted
  //   shortcuts since our install. So on updates we only update if shortcut
  //   already exists)
  bool ret1 = true;
  std::wstring chrome_link(shortcut_path);  // Chrome link (launches Chrome)
  file_util::AppendToPath(&chrome_link, product_name + L".lnk");
  std::wstring chrome_exe(install_path);  // Chrome link target
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);

  if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer_util::INSTALL_REPAIRED) ||
      (old_shortcuts_existed)) {
    if (!file_util::PathExists(shortcut_path))
      file_util::CreateDirectoryW(shortcut_path);

    LOG(INFO) << "Creating shortcut to " << chrome_exe << " at " << chrome_link;
    ShellUtil::UpdateChromeShortcut(chrome_exe, chrome_link, true);
  } else if (file_util::PathExists(chrome_link)) {
    LOG(INFO) << "Updating shortcut at " << chrome_link
              << " to point to " << chrome_exe;
    ShellUtil::UpdateChromeShortcut(chrome_exe, chrome_link, false);
  }

  // Create/update uninstall link
  bool ret2 = true;
  std::wstring uninstall_link(shortcut_path);  // Uninstall Chrome link

  file_util::AppendToPath(&uninstall_link,
      installer_util::GetLocalizedString(IDS_UNINSTALL_CHROME_BASE) + L".lnk");
  if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer_util::INSTALL_REPAIRED) ||
      (old_shortcuts_existed) ||
      (file_util::PathExists(uninstall_link))) {
    if (!file_util::PathExists(shortcut_path))
      file_util::CreateDirectoryW(shortcut_path);
    std::wstring setup_exe(installer::GetInstallerPathUnderChrome(install_path,
                                                                  new_version));
    file_util::AppendToPath(&setup_exe,
                            file_util::GetFilenameFromPath(exe_path));
    std::wstring arguments(L" --");
    arguments.append(installer_util::switches::kUninstall);
    LOG(INFO) << "Creating/updating uninstall link at " << uninstall_link;
    std::wstring target_folder = file_util::GetDirectoryFromPath(install_path);
    ret2 = file_util::CreateShortcutLink(setup_exe.c_str(),
                                         uninstall_link.c_str(),
                                         target_folder.c_str(),
                                         arguments.c_str(),
                                         NULL,
                                         setup_exe.c_str(),
                                         0);
  }

  // Update Desktop and Quick Launch shortcuts (only if they already exist)
  UpdateChromeExeShortcuts(chrome_exe);

  return ret1 && ret2;
}
}  // namespace


std::wstring installer::GetInstallerPathUnderChrome(
    const std::wstring& install_path, const std::wstring& new_version) {
  std::wstring installer_path(install_path);
  file_util::AppendToPath(&installer_path, new_version);
  file_util::AppendToPath(&installer_path, installer::kInstallerDir);
  return installer_path;
}


installer_util::InstallStatus installer::InstallOrUpdateChrome(
    const std::wstring& exe_path, const std::wstring& archive_path,
    const std::wstring& install_temp_path, bool system_install,
    const Version& new_version, const Version* installed_version) {

  std::wstring install_path(GetChromeInstallPath(system_install));
  if (install_path.empty()) {
    LOG(ERROR) << "Couldn't get installation destination path";
    return installer_util::INSTALL_FAILED;
  } else {
    LOG(INFO) << "install destination path: " << install_path;
  }

  std::wstring src_path(install_temp_path);
  file_util::AppendToPath(&src_path, std::wstring(kInstallSourceDir));
  file_util::AppendToPath(&src_path, std::wstring(kInstallSourceChromeDir));

  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  bool install_success = InstallNewVersion(exe_path, archive_path, src_path,
      install_path, install_temp_path, reg_root, new_version);

  installer_util::InstallStatus result;
  if (!install_success) {
    LOG(ERROR) << "Install failed.";
    result = installer_util::INSTALL_FAILED;
  } else {
    if (!installed_version) {
      LOG(INFO) << "First install of version " << new_version.GetString();
      result = installer_util::FIRST_INSTALL_SUCCESS;
    } else if (new_version.GetString() == installed_version->GetString()) {
      LOG(INFO) << "Install repaired of version " << new_version.GetString();
      result = installer_util::INSTALL_REPAIRED;
    } else if (new_version.IsHigherThan(installed_version)) {
      LOG(INFO) << "Version updated to " << new_version.GetString();
      result = installer_util::NEW_VERSION_UPDATED;
    } else {
      NOTREACHED();
    }

    if (!CreateOrUpdateChromeShortcuts(exe_path, system_install, result,
                                       install_path, new_version.GetString()))
      LOG(WARNING) << "Failed to create/update start menu shortcut.";

    std::wstring chrome_exe(install_path);
    file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
    if (result == installer_util::FIRST_INSTALL_SUCCESS ||
        result == installer_util::INSTALL_REPAIRED) {
      // Try to add Chrome to Media Player shim inclusion list. We don't do any
      // error checking here because this operation will fail if user doesn't
      // have admin rights and we want to ignore the error.
      AddChromeToMediaPlayerList();

      // We try to register Chrome as a valid browser on local machine. This
      // will work only if current user has admin rights.
      LOG(INFO) << "Registering Chrome as browser";
      ShellUtil::RegisterStatus ret =
          ShellUtil::AddChromeToSetAccessDefaults(chrome_exe, true);
      LOG(ERROR) << "Return status of Chrome browser registration " << ret;
    } else {
      UpdateChromeExeShortcuts(chrome_exe);
      RemoveOldVersionDirs(install_path, new_version.GetString());
      // Delete the old key for Uninstall link (this code can be removed once
      // everyone has migrated to the new "Google Chrome" version of the key).
      RegKey key(reg_root, L"", KEY_ALL_ACCESS);
      key.DeleteKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chrome");
      key.Close();
    }
  }

  return result;
}
