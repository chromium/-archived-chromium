// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the methods useful for uninstalling Chrome.

#include "chrome/installer/setup/uninstall.h"

#include <atlbase.h>
#include <windows.h>
#include <msi.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "base/wmi_util.h"
#include "chrome/app/result_codes.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/setup/setup.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

#include "installer_util_strings.h"

namespace {

// This method deletes Chrome shortcut folder from Windows Start menu. It
// checks system_uninstall to see if the shortcut is in all users start menu
// or current user start menu.
void DeleteChromeShortcut(bool system_uninstall) {
  std::wstring shortcut_path;
  if (system_uninstall) {
    PathService::Get(base::DIR_COMMON_START_MENU, &shortcut_path);
    // In case of system level uninstall, we want to remove desktop and
    // Quick Launch shortcuts also. In case of user level uninstall,
    // chrome.exe deletes these shortcuts.
    ShellUtil::RemoveChromeDesktopShortcut(ShellUtil::CURRENT_USER |
                                           ShellUtil::SYSTEM_LEVEL);
    ShellUtil::RemoveChromeQuickLaunchShortcut(ShellUtil::CURRENT_USER |
                                               ShellUtil::SYSTEM_LEVEL);
  } else {
    PathService::Get(base::DIR_START_MENU, &shortcut_path);
  }
  if (shortcut_path.empty()) {
    LOG(ERROR) << "Failed to get location for shortcut.";
  } else {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    file_util::AppendToPath(&shortcut_path, dist->GetApplicationName());
    LOG(INFO) << "Deleting shortcut " << shortcut_path;
    if (!file_util::Delete(shortcut_path, true))
      LOG(ERROR) << "Failed to delete folder: " << shortcut_path;
  }
}

// Deletes all installed files of Chromium and Folders. Before deleting it
// needs to move setup.exe in a temp folder because the current process
// is using that file. It returns false when it can not get the path to
// installation folder, in all other cases it returns true even in case
// of error (only logs the error).
bool DeleteFilesAndFolders(const std::wstring& exe_path, bool system_uninstall,
    const installer::Version& installed_version) {
  std::wstring install_path(installer::GetChromeInstallPath(system_uninstall));
  if (install_path.empty()) {
    LOG(ERROR) << "Could not get installation destination path.";
    return false; // Nothing else we can do for uninstall, so we return.
  } else {
    LOG(INFO) << "install destination path: " << install_path;
  }

  std::wstring setup_exe(installer::GetInstallerPathUnderChrome(
      install_path, installed_version.GetString()));
  file_util::AppendToPath(&setup_exe, file_util::GetFilenameFromPath(exe_path));

  std::wstring temp_file;
  file_util::CreateTemporaryFileName(&temp_file);
  file_util::Move(setup_exe, temp_file);

  LOG(INFO) << "Deleting install path " << install_path;
  if (!file_util::Delete(install_path, true)) {
    LOG(ERROR) << "Failed to delete folder (1st try): " << install_path;
    // This is to let any closing chrome.exe die before trying delete one
    // more time.
    Sleep(10000);
    if (!file_util::Delete(install_path, true))
      LOG(ERROR) << "Failed to delete folder (2nd try): " << install_path;
  }

  // Now check and delete if the parent directories are empty
  // For example Google\Chrome or Chromium
  std::wstring parent_dir = file_util::GetDirectoryFromPath(install_path);
  if (!parent_dir.empty() && file_util::IsDirectoryEmpty(parent_dir)) {
    if (!file_util::Delete(parent_dir, true))
      LOG(ERROR) << "Failed to delete folder: " << parent_dir;
    parent_dir = file_util::GetDirectoryFromPath(parent_dir);
    if (!parent_dir.empty() &&
        file_util::IsDirectoryEmpty(parent_dir)) {
      if (!file_util::Delete(parent_dir, true))
        LOG(ERROR) << "Failed to delete folder: " << parent_dir;
    }
  }
  return true;
}

// This method tries to delete a registry key and logs an error message
// in case of failure. It returns true if deletion is successful,
// otherwise false.
bool DeleteRegistryKey(RegKey& key, const std::wstring& key_path) {
  LOG(INFO) << "Deleting registry key " << key_path;
  if (!key.DeleteKey(key_path.c_str())) {
    LOG(ERROR) << "Failed to delete registry key: " << key_path;
    return false;
  }
  return true;
}

// This method tries to delete a registry value and logs an error message
// in case of failure. It returns true if deletion is successful,
// otherwise false.
bool DeleteRegistryValue(HKEY reg_root, const std::wstring& key_path,
                         const std::wstring& value_name) {
  RegKey key(reg_root, key_path.c_str(), KEY_ALL_ACCESS);
  LOG(INFO) << "Deleting registry value " << value_name;
  if (!key.DeleteValue(value_name.c_str())) {
    LOG(ERROR) << "Failed to delete registry value: " << value_name;
    return false;
  }
  return true;
}

// This method checks if Chrome is currently running or if the user has
// cancelled the uninstall operation by clicking Cancel on the confirmation
// box that Chrome pops up.
installer_util::InstallStatus IsChromeActiveOrUserCancelled(
    bool system_uninstall) {
  static const std::wstring kCmdLineOptions(L" --uninstall");
  int32 exit_code = ResultCodes::NORMAL_EXIT;

  // Here we want to save user from frustration (in case of Chrome crashes)
  // and continue with the uninstallation as long as chrome.exe process exit
  // code is NOT one of the following:
  // - UNINSTALL_CHROME_ALIVE - chrome.exe is currently running
  // - UNINSTALL_USER_CANCEL - User cancelled uninstallation
  // - HUNG - chrome.exe was killed by HuntForZombieProcesses() (until we can
  //          give this method some brains and not kill chrome.exe launched
  //          by us, we will not uninstall if we get this return code).
  LOG(INFO) << "Launching Chrome to do uninstall tasks.";
  if (installer::LaunchChromeAndWaitForResult(system_uninstall,
                                              kCmdLineOptions,
                                              &exit_code)) {
    LOG(INFO) << "chrome.exe launched for uninstall confirmation returned: "
              << exit_code;
    if ((exit_code == ResultCodes::UNINSTALL_CHROME_ALIVE) ||
        (exit_code == ResultCodes::UNINSTALL_USER_CANCEL) ||
        (exit_code == ResultCodes::HUNG))
      return installer_util::UNINSTALL_CANCELLED;
  } else {
    LOG(ERROR) << "Failed to launch chrome.exe for uninstall confirmation.";
  }

  return installer_util::UNINSTALL_CONFIRMED;
}
}  // namespace


installer_util::InstallStatus installer_setup::UninstallChrome(
    const std::wstring& exe_path, bool system_uninstall,
    const installer::Version& installed_version,
    bool remove_all, bool force_uninstall) {
  if (!force_uninstall) {
    installer_util::InstallStatus status =
        IsChromeActiveOrUserCancelled(system_uninstall);
    if (status != installer_util::UNINSTALL_CONFIRMED)
      return status;
  }

#if defined(GOOGLE_CHROME_BUILD)
  // TODO(rahulk): This should be done by DoPreUninstallOperations call above
  wchar_t product[39];  // GUID + '\0'
  MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);  // Don't show any UI to user.
  for (int i = 0; MsiEnumRelatedProducts(google_update::kGearsUpgradeCode, 0, i,
                                         product) != ERROR_NO_MORE_ITEMS; ++i) {
    LOG(INFO) << "Uninstalling Gears - " << product;
    unsigned int ret = MsiConfigureProduct(product, INSTALLLEVEL_MAXIMUM,
                                           INSTALLSTATE_ABSENT);
    if (ret != ERROR_SUCCESS)
      LOG(ERROR) << "Failed to uninstall Gears " << product << ": " << ret;
  }
#endif

  // Chrome is not in use so lets uninstall Chrome by deleting various files
  // and registry entries. Here we will just make best effort and keep going
  // in case of errors.
  // First delete shortcut from Start->Programs.
  DeleteChromeShortcut(system_uninstall);

  // Delete the registry keys (Uninstall key and Version key).
  HKEY reg_root = system_uninstall ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key(reg_root, L"", KEY_ALL_ACCESS);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  DeleteRegistryKey(key, dist->GetUninstallRegPath());
  DeleteRegistryKey(key, dist->GetVersionKey());

  // Delete Software\Classes\ChromeHTML,
  // Software\Clients\StartMenuInternet\chrome.exe and
  // Software\RegisteredApplications\Chrome
  std::wstring html_prog_id(ShellUtil::kRegClasses);
  file_util::AppendToPath(&html_prog_id, ShellUtil::kChromeHTMLProgId);
  DeleteRegistryKey(key, html_prog_id);

  std::wstring set_access_key(ShellUtil::kRegStartMenuInternet);
  file_util::AppendToPath(&set_access_key, installer_util::kChromeExe);
  DeleteRegistryKey(key, set_access_key);

  DeleteRegistryValue(reg_root, ShellUtil::kRegRegisteredApplications,
                      dist->GetApplicationName());
  key.Close();

  // Delete shared registry keys as well (these require admin rights) if
  // remove_all option is specified.
  if (remove_all) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, L"", KEY_ALL_ACCESS);
    DeleteRegistryKey(hklm_key, set_access_key);
    DeleteRegistryKey(hklm_key, html_prog_id);
    DeleteRegistryValue(HKEY_LOCAL_MACHINE,
                        ShellUtil::kRegRegisteredApplications,
                        dist->GetApplicationName());

    // Delete media player registry key that exists only in HKLM.
    std::wstring reg_path(installer::kMediaPlayerRegPath);
    file_util::AppendToPath(&reg_path, installer_util::kChromeExe);
    DeleteRegistryKey(hklm_key, reg_path);
    hklm_key.Close();
  }

  // Finally delete all the files from Chrome folder after moving setup.exe
  // to a temp location.
  if (!DeleteFilesAndFolders(exe_path, system_uninstall, installed_version))
    return installer_util::UNINSTALL_FAILED;

  LOG(INFO) << "Uninstallation complete. Launching Uninstall survey.";
  dist->DoPostUninstallOperations(installed_version);
  return installer_util::UNINSTALL_SUCCESSFUL;
}

