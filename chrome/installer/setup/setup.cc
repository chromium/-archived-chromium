// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>

#include "chrome/installer/setup/setup.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"
#include "chrome/installer/util/work_item_list.h"

#include "installer_util_strings.h"

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
    LOG(ERROR) << "Could not add Chrome to media player inclusion list.";

}

void DoFirstInstallTasks(std::wstring install_path, int options) {
  bool system_level = (options & installer_util::SYSTEM_LEVEL) != 0;
  // Try to add Chrome to Media Player shim inclusion list. We don't do any
  // error checking here because this operation will fail if user doesn't
  // have admin rights and we want to ignore the error.
  AddChromeToMediaPlayerList();

  // We try to register Chrome as a valid browser on local machine. This
  // will work only if current user has admin rights.
  std::wstring chrome_exe(install_path);
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
  CommandLine cmd_line;
  LOG(INFO) << "Registering Chrome as browser";
  ShellUtil::RegisterStatus ret = ShellUtil::FAILURE;
  if (options & installer_util::MAKE_CHROME_DEFAULT) {
    ret = ShellUtil::AddChromeToSetAccessDefaults(chrome_exe, false);
    if (ret == ShellUtil::SUCCESS) {
      if (system_level) {
        ShellUtil::MakeChromeDefault(
            ShellUtil::CURRENT_USER | ShellUtil::SYSTEM_LEVEL, chrome_exe);
      } else {
        ShellUtil::MakeChromeDefault(ShellUtil::CURRENT_USER, chrome_exe);
      }
    }
  } else {
    ret = ShellUtil::AddChromeToSetAccessDefaults(chrome_exe, true);
  }
  LOG(INFO) << "Return status of Chrome browser registration " << ret;
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
                                   int options,
                                   installer_util::InstallStatus install_status,
                                   const std::wstring& install_path,
                                   const std::wstring& new_version) {
  bool system_install = (options & installer_util::SYSTEM_LEVEL) != 0;
  std::wstring shortcut_path;
  int dir_enum = (system_install) ? base::DIR_COMMON_START_MENU :
                                    base::DIR_START_MENU;
  if (!PathService::Get(dir_enum, &shortcut_path)) {
    LOG(ERROR) << "Failed to get location for shortcut.";
    return false;
  }

  // The location of Start->Programs->Google Chrome folder
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  const std::wstring& product_name = dist->GetApplicationName();
  file_util::AppendToPath(&shortcut_path, product_name);

  // Create/update Chrome link (points to chrome.exe) & Uninstall Chrome link
  // (which points to setup.exe) under this folder only if:
  // - This is a new install or install repair
  // OR
  // - The shortcut already exists in case of updates (user may have deleted
  //   shortcuts since our install. So on updates we only update if shortcut
  //   already exists)
  bool ret = true;
  std::wstring chrome_link(shortcut_path);  // Chrome link (launches Chrome)
  file_util::AppendToPath(&chrome_link, product_name + L".lnk");
  std::wstring chrome_exe(install_path);  // Chrome link target
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);

  if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer_util::INSTALL_REPAIRED)) {
    if (!file_util::PathExists(shortcut_path))
      file_util::CreateDirectoryW(shortcut_path);

    LOG(INFO) << "Creating shortcut to " << chrome_exe << " at " << chrome_link;
    ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe, chrome_link, true);
  } else if (file_util::PathExists(chrome_link)) {
    LOG(INFO) << "Updating shortcut at " << chrome_link
              << " to point to " << chrome_exe;
    ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe,
                                                 chrome_link,
                                                 false); // do not create new
  }

  // Create/update uninstall link
  std::wstring uninstall_link(shortcut_path);  // Uninstall Chrome link
  file_util::AppendToPath(&uninstall_link,
      dist->GetUninstallLinkName() + L".lnk");
  if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer_util::INSTALL_REPAIRED) ||
      (file_util::PathExists(uninstall_link))) {
    if (!file_util::PathExists(shortcut_path))
      file_util::CreateDirectoryW(shortcut_path);
    std::wstring setup_exe(installer::GetInstallerPathUnderChrome(install_path,
                                                                  new_version));
    file_util::AppendToPath(&setup_exe,
                            file_util::GetFilenameFromPath(exe_path));
    std::wstring arguments(L" --");
    arguments.append(installer_util::switches::kUninstall);
    if (system_install) {
      arguments.append(L" --");
      arguments.append(installer_util::switches::kSystemLevel);
    }

    LOG(INFO) << "Creating/updating uninstall link at " << uninstall_link;
    std::wstring target_folder = file_util::GetDirectoryFromPath(install_path);
    ret = ret && file_util::CreateShortcutLink(setup_exe.c_str(),
                                               uninstall_link.c_str(),
                                               target_folder.c_str(),
                                               arguments.c_str(),
                                               NULL, setup_exe.c_str(), 0);
  }

  // Update Desktop and Quick Launch shortcuts. If --create-new-shortcuts
  // is specified we want to create them, otherwise we update them only if
  // they exist.
  bool create = (options & installer_util::CREATE_ALL_SHORTCUTS) != 0;
  if (system_install) {
    ret = ret && ShellUtil::CreateChromeDesktopShortcut(chrome_exe,
        ShellUtil::SYSTEM_LEVEL, create);
    ret = ret && ShellUtil::CreateChromeQuickLaunchShortcut(chrome_exe,
        ShellUtil::CURRENT_USER | ShellUtil::SYSTEM_LEVEL, create);
  } else {
    ret = ret && ShellUtil::CreateChromeDesktopShortcut(chrome_exe,
        ShellUtil::CURRENT_USER, create);
    ret = ret && ShellUtil::CreateChromeQuickLaunchShortcut(chrome_exe,
        ShellUtil::CURRENT_USER, create);
  }

  return ret;
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
    const std::wstring& install_temp_path, int options,
    const Version& new_version, const Version* installed_version) {
  bool system_install = (options & installer_util::SYSTEM_LEVEL) != 0;
  std::wstring install_path(GetChromeInstallPath(system_install));
  if (install_path.empty()) {
    LOG(ERROR) << "Could not get installation destination path.";
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

    if (!CreateOrUpdateChromeShortcuts(exe_path, options, result,
                                       install_path, new_version.GetString()))
      LOG(WARNING) << "Failed to create/update start menu shortcut.";

    if (result == installer_util::FIRST_INSTALL_SUCCESS ||
        result == installer_util::INSTALL_REPAIRED) {
      DoFirstInstallTasks(install_path, options);
    } else {
      RemoveOldVersionDirs(install_path, new_version.GetString());
    }
  }

  return result;
}
