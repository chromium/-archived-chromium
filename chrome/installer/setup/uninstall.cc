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
#include "chrome/app/google_update_settings.h"
#include "chrome/app/result_codes.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

#include "setup_resource.h"
#include "setup_strings.h"

namespace {

// This method deletes Chrome shortcut folder from Windows Start menu. It
// checks system_uninstall to see if the shortcut is in all users start menu
// or current user start menu.
void DeleteChromeShortcut(bool system_uninstall) {
  std::wstring shortcut_path;
  if (system_uninstall) {
    PathService::Get(base::DIR_COMMON_START_MENU, &shortcut_path);
  } else {
    PathService::Get(base::DIR_START_MENU, &shortcut_path);
  }
  if (shortcut_path.empty()) {
    LOG(ERROR) << "failed to get location for shortcut";
  } else {
    file_util::AppendToPath(&shortcut_path,
        installer_util::GetLocalizedString(IDS_PRODUCT_NAME_BASE));
    LOG(INFO) << "Deleting shortcut " << shortcut_path;
    if (!file_util::Delete(shortcut_path, true))
      LOG(ERROR) << "Failed to delete folder: " << shortcut_path;
  }
}

// This method tries to delete a registry key and logs an error message
// in case of failure. It returns true if deletion is successful,
// otherwise false.
bool DeleteRegistryKey(RegKey& key, const std::wstring& key_path) {
  LOG(INFO) << "Deleting registry key " << key_path;
  if (!key.DeleteKey(key_path.c_str())) {
    LOG(ERROR) << "Failed to delete registry key: " << key_path
               << " and the error is " << InstallUtil::FormatLastWin32Error();
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
    LOG(ERROR) << "Failed to delete registry value: " << value_name
               << " and the error is " << InstallUtil::FormatLastWin32Error();
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
  static const int32 kTimeOutMs = 30000;
  int32 exit_code = ResultCodes::NORMAL_EXIT;
  bool is_timeout = false;

  // We ignore all other errors such as whether launching chrome fails,
  // whether chrome returns UNINSTALL_ERROR, etc.
  LOG(INFO) << "Launching Chrome to do uninstall tasks.";
  if (installer::LaunchChromeAndWaitForResult(system_uninstall,
                                              kCmdLineOptions,
                                              kTimeOutMs,
                                              &exit_code,
                                              &is_timeout)) {
    if (is_timeout || exit_code == ResultCodes::UNINSTALL_CHROME_ALIVE) {
      LOG(ERROR) << "Can't uninstall when chrome is still running";
      return installer_util::CHROME_RUNNING;
    } else if (exit_code == ResultCodes::UNINSTALL_USER_CANCEL) {
      LOG(INFO) << "User cancelled uninstall operation";
      return installer_util::UNINSTALL_CANCELLED;
    } else if (exit_code == ResultCodes::UNINSTALL_ERROR) {
      LOG(ERROR) << "chrome.exe reported error while uninstalling.";
      return installer_util::UNINSTALL_FAILED;
    }
  }

  return installer_util::UNINSTALL_CONFIRMED;
}

// Read the URL from the resource file and substitute the locale parameter
// with whatever Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
std::wstring GetUninstallSurveyUrl() {
  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(
      _AtlBaseModule.GetModuleInstance(), IDS_UNINSTALL_SURVEY_URL);
  DCHECK(image);
  std::wstring url = std::wstring(image->achString, image->nLength);
  DCHECK(!url.empty());

  std::wstring language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.

  return ReplaceStringPlaceholders(url.c_str(), language.c_str(), NULL);
}

// This method launches an uninstall survey and is called at the end of
// uninstall process. We are not doing any error checking here as it is
// not critical to have this survey. If we fail to launch it, we just
// ignore it silently.
void LaunchUninstallSurvey(const installer::Version& installed_version) {
  // Send the Chrome version and OS version as params to the form.
  // It would be nice to send the locale, too, but I don't see an
  // easy way to get that in the existing code. It's something we
  // can add later, if needed.
  // We depend on installed_version.GetString() not having spaces or other
  // characters that need escaping: 0.2.13.4. Should that change, we will
  // need to escape the string before using it in a URL.
  const std::wstring kVersionParam = L"crversion";
  const std::wstring kVersion = installed_version.GetString();
  const std::wstring kOSParam = L"os";
  std::wstring os_version = L"na";
  OSVERSIONINFO version_info;
  version_info.dwOSVersionInfoSize = sizeof version_info;
  if (GetVersionEx(&version_info)) {
    os_version = StringPrintf(L"%d.%d.%d",
        version_info.dwMajorVersion,
        version_info.dwMinorVersion,
        version_info.dwBuildNumber);
  }

  std::wstring iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  file_util::AppendToPath(&iexplore, L"Internet Explorer");
  file_util::AppendToPath(&iexplore, L"iexplore.exe");

  std::wstring command = iexplore + L" " + GetUninstallSurveyUrl() + L"&" +
      kVersionParam + L"=" + kVersion + L"&" + kOSParam + L"=" + os_version;
  int pid = 0;
  WMIProcessUtil::Launch(command, &pid);
}

// Uninstall Chrome specific Gears. First we find Gears MSI ProductId (that
// changes with every new version of Gears) using Gears MSI UpgradeCode (that
// does not change) and then uninstall Gears using API.
void UninstallGears() {
  wchar_t product[39];  // GUID + '\0'
  MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);  // Don't show any UI to user.
  for (int i = 0; MsiEnumRelatedProducts(google_update::kGearsUpgradeCode, 0, i,
                                         product) != ERROR_NO_MORE_ITEMS; ++i) {
    LOG(INFO) << "Uninstalling Gears - " << product;
    unsigned int ret = MsiConfigureProduct(product, INSTALLLEVEL_MAXIMUM,
                                           INSTALLSTATE_ABSENT);
    if (ret != ERROR_SUCCESS)
      LOG(ERROR) << "Failed to uninstall Gears " << product
                 << " because of error " << ret;
  }
}

}  // namespace


installer_util::InstallStatus installer_setup::UninstallChrome(
    const std::wstring& exe_path, bool system_uninstall,
    const installer::Version& installed_version, bool remove_all) {
  installer_util::InstallStatus status =
      IsChromeActiveOrUserCancelled(system_uninstall);
  if (status == installer_util::CHROME_RUNNING ||
      status == installer_util::UNINSTALL_CANCELLED)
    return status;

  // Uninstall Gears first.
  UninstallGears();

  // Chrome is not in use so lets uninstall Chrome by deleting various files
  // and registry entries. Here we will just make best effort and keep going
  // in case of errors.
  // First delete shortcut from Start->Programs.
  DeleteChromeShortcut(system_uninstall);

  // Delete the registry keys (Uninstall key and Google Update update key).
  HKEY reg_root = system_uninstall ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key(reg_root, L"", KEY_ALL_ACCESS);
  DeleteRegistryKey(key, installer_util::kUninstallRegPath);
  DeleteRegistryKey(key, InstallUtil::GetChromeGoogleUpdateKey());

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
                      installer_util::kApplicationName);
  key.Close();

  // Delete shared registry keys as well (these require admin rights) if
  // remove_all option is specified.
  if (remove_all) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, L"", KEY_ALL_ACCESS);
    DeleteRegistryKey(hklm_key, set_access_key);
    DeleteRegistryKey(hklm_key, html_prog_id);
    DeleteRegistryValue(HKEY_LOCAL_MACHINE,
                        ShellUtil::kRegRegisteredApplications,
                        installer_util::kApplicationName);

    // Delete media player registry key that exists only in HKLM.
    std::wstring reg_path(installer::kMediaPlayerRegPath);
    file_util::AppendToPath(&reg_path, installer_util::kChromeExe);
    DeleteRegistryKey(hklm_key, reg_path);
    hklm_key.Close();
  }

  // Finally delete all the files from Chrome folder after moving setup.exe
  // to a temp location.
  std::wstring install_path(installer::GetChromeInstallPath(system_uninstall));
  if (install_path.empty()) {
    LOG(ERROR) << "Couldn't get installation destination path";
    // Nothing else we could do for uninstall, so we return.
    return installer_util::UNINSTALL_FAILED;
  } else {
    LOG(INFO) << "install destination path: " << install_path;
  }

  std::wstring setup_exe(install_path);
  file_util::AppendToPath(&setup_exe, installed_version.GetString());
  file_util::AppendToPath(&setup_exe, installer::kInstallerDir);
  file_util::AppendToPath(&setup_exe, file_util::GetFilenameFromPath(exe_path));

  std::wstring temp_file;
  file_util::CreateTemporaryFileName(&temp_file);
  file_util::Move(setup_exe, temp_file);

  LOG(INFO) << "Deleting install path " << install_path;
  if (!file_util::Delete(install_path, true))
    LOG(ERROR) << "Failed to delete folder: " << install_path;

  LOG(INFO) << "Uninstallation complete. Launching Uninstall survey.";
  LaunchUninstallSurvey(installed_version);
  return installer_util::UNINSTALL_SUCCESSFUL;
}
