// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/chrome_mini_installer.h"

#include "base/path_service.h"
#include "base/process_util.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// Installs the Chrome mini-installer, checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMiniInstaller(bool over_install) {
  std::wstring mini_installer_path = GetMiniInstallerExePath();
  printf("\nChrome will be installed at %ls level\n", install_type_.c_str());
  printf("\nWill proceed with the test only if mini_installer.exe exists\n");
  ASSERT_TRUE(file_util::PathExists(mini_installer_path));
  printf("\nmini_installer found at %ls\n", mini_installer_path.c_str());
  LaunchInstaller(mini_installer_path,
                  mini_installer_constants::kChromeMiniInstallerExecutable);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey()));
  FindChromeShortcut();
  VerifyChromeLaunch();
  CloseFirstRunUIDialog(over_install);
  }

// Installs chromesetup.exe, waits for the install to finish and then
// checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMetaInstaller() {
  // Install Google Chrome through meta installer.
  LaunchInstaller(mini_installer_constants::kChromeMetaInstallerExeLocation,
                  mini_installer_constants::kChromeSetupExecutable);
  WaitUntilProcessStopsRunning(
      mini_installer_constants::kChromeMetaInstallerExecutable);
  std::wstring chrome_google_update_state_key(
      google_update::kRegPathClientState);
  chrome_google_update_state_key.append(L"\\");
  chrome_google_update_state_key.append(google_update::kChromeGuid);
  ASSERT_TRUE(CheckRegistryKey(chrome_google_update_state_key));
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey()));
  FindChromeShortcut();
  VerifyChromeLaunch();
  WaitUntilProcessStartsRunning(installer_util::kChromeExe);
  ASSERT_TRUE(CloseWindow(mini_installer_constants::kChromeFirstRunUI,
                          WM_CLOSE));
}

// If the build type is Google Chrome, then it first installs meta installer
// and then over installs with mini_installer. It also verifies if Chrome can
// be launched successfully after overinstall.
void ChromeMiniInstaller::OverInstall() {
  if (!IsChromiumBuild()) {
    InstallMetaInstaller();
    // gets the registry key value before overinstall.
    std::wstring reg_key_value_returned = GetRegistryKey();
    printf("\n\nPreparing to overinstall...\n");
    std::wstring mini_installer_path = GetMiniInstallerExePath();
    printf("\nOverinstall path is %ls\n",  mini_installer_path.c_str());
    InstallMiniInstaller(true);
    // Get the registry key value after over install
    std::wstring reg_key_value_after_overinstall = GetRegistryKey();
    ASSERT_TRUE(VerifyOverInstall(reg_key_value_returned,
                                  reg_key_value_after_overinstall));
  } else {
    printf("\n\nThis test doesn't run on a chromium build\n");
  }
}

// This method first checks if Chrome is running.
// If yes, will close all the processes.
// Then will find and spawn uninstall path.
// Handles uninstall confirm dialog.
// Waits until setup.exe ends.
// Checks if registry key exist even after uninstall.
// Deletes App dir.
// Closes feedback form.
void ChromeMiniInstaller::UnInstall() {
  printf("\n\nVerifying if Chrome is installed...\n\n");
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!CheckRegistryKey(dist->GetVersionKey())) {
    printf("Chrome is not installed.\n");
    return;
  }
  printf("\n\nUninstalling Chrome...\n");
  printf("Closing Chrome processes, if any...\n");
  CloseProcesses(installer_util::kChromeExe);
  std::wstring uninstall_path = GetUninstallPath();
  ASSERT_TRUE(file_util::PathExists(uninstall_path));
  std::wstring uninstall_args = L"\"" + uninstall_path +
                                L"\"" + L" -uninstall";
  if (install_type_ == mini_installer_constants::kSystemInstall)
    uninstall_args = uninstall_args + L" -system-level";
  base::LaunchApp(uninstall_args, false, false, NULL);
  printf("Launched setup.exe -uninstall....\n");
  ASSERT_TRUE(CloseWindow(mini_installer_constants::kChromeBuildType,
                          WM_COMMAND));
  WaitUntilProcessStopsRunning(
      mini_installer_constants::kChromeSetupExecutable);
  ASSERT_FALSE(CheckRegistryKey(dist->GetVersionKey()));
  DeleteAppFolder();
  FindChromeShortcut();
  CloseProcesses(mini_installer_constants::kIEExecutable);
  ASSERT_EQ(0,
      base::GetProcessCount(mini_installer_constants::kIEExecutable, NULL));
}

// Takes care of Chrome uninstall dialog.
bool ChromeMiniInstaller::CloseWindow(LPCWSTR window_name, UINT message) {
  int timer = 0;
  bool return_val = false;
  HWND hndl = FindWindow(NULL, window_name);
  while (hndl== NULL && (timer < 60000)) {
    hndl = FindWindow(NULL, window_name);
    Sleep(200);
    timer = timer + 200;
  }
  if (hndl != NULL) {
    LRESULT _result = SendMessage(hndl, message, 1, 0);
    return_val = true;
  }
  return return_val;
}

// Closes Chrome browser.
void ChromeMiniInstaller::CloseChromeBrowser(LPCWSTR window_name) {
  WaitUntilProcessStartsRunning(installer_util::kChromeExe);
  ASSERT_TRUE(CloseWindow(window_name, WM_CLOSE));
}

// Closes the First Run UI dialog.
void ChromeMiniInstaller::CloseFirstRunUIDialog(bool over_install) {
  WaitUntilProcessStartsRunning(installer_util::kChromeExe);
  if (!over_install) {
    ASSERT_TRUE(CloseWindow(mini_installer_constants::kChromeFirstRunUI,
                            WM_CLOSE));
  } else {
    ASSERT_TRUE(CloseWindow(mini_installer_constants::kBrowserTabName,
                            WM_CLOSE));
  }
}

// Checks for all requested running processes and kills them.
void ChromeMiniInstaller::CloseProcesses(const std::wstring& executable_name) {
  int timer = 0;
  while ((base::GetProcessCount(executable_name, NULL) > 0) &&
         (timer < 20000)) {
    base::KillProcesses(executable_name, 1, NULL);
    Sleep(200);
    timer = timer + 200;
  }
  ASSERT_EQ(0, base::GetProcessCount(executable_name, NULL));
}

// Checks for Chrome registry keys.
bool ChromeMiniInstaller::CheckRegistryKey(std::wstring key_path) {
  RegKey key;
  if (!key.Open(GetRootRegistryKey(), key_path.c_str(), KEY_ALL_ACCESS)) {
    printf("Cannot open reg key\n");
    return false;
  }
  std::wstring reg_key_value_returned = GetRegistryKey();
  printf("Reg key value is%ls\n",  reg_key_value_returned.c_str());
  return true;
}

// Deletes App folder after uninstall.
void ChromeMiniInstaller::DeleteAppFolder() {
  std::wstring path = GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::UpOneDirectory(&path);
  printf("Deleting this path after uninstall%ls\n",  path.c_str());
  if (file_util::PathExists(path))
    ASSERT_TRUE(file_util::Delete(path.c_str(), true));
}

// Verifies if Chrome shortcut exists.
void ChromeMiniInstaller::FindChromeShortcut() {
  std::wstring username, path, append_path, uninstall_lnk, shortcut_path;
  bool return_val = false;
  path = GetStartMenuShortcutPath();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeBuildType);
  // Verify if path exists.
  if (file_util::PathExists(path)) {
    return_val = true;
    uninstall_lnk = path;
    file_util::AppendToPath(&path,
                            mini_installer_constants::kChromeLaunchShortcut);
    file_util::AppendToPath(&uninstall_lnk,
                            mini_installer_constants::kChromeUninstallShortcut);
    ASSERT_TRUE(file_util::PathExists(path));
    ASSERT_TRUE(file_util::PathExists(uninstall_lnk));
  }
  if (return_val)
    printf("Chrome shortcuts found are:\n%ls\n%ls\n\n",
           path.c_str(), uninstall_lnk.c_str());
  else
    printf("Chrome shortcuts not found\n\n");
}

// This method returns path to either program files
// or documents and setting based on the install type.
std::wstring ChromeMiniInstaller::GetChromeInstallDirectoryLocation() {
  std::wstring path;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    PathService::Get(base::DIR_PROGRAM_FILES, &path);
  else
    PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
  return path;
}

// Get path for mini_installer.exe.
std::wstring ChromeMiniInstaller::GetMiniInstallerExePath() {
  std::wstring mini_installer_path;
  PathService::Get(base::DIR_EXE, &mini_installer_path);
  file_util::AppendToPath(&mini_installer_path,
             mini_installer_constants::kChromeMiniInstallerExecutable);
  return mini_installer_path;
}

// This method gets the shortcut path  from startmenu based on install type
std::wstring ChromeMiniInstaller::GetStartMenuShortcutPath() {
  std::wstring path_name;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    PathService::Get(base::DIR_COMMON_START_MENU, &path_name);
  else
    PathService::Get(base::DIR_START_MENU, &path_name);
  return path_name;
}

// Gets the path for uninstall.
std::wstring ChromeMiniInstaller::GetUninstallPath() {
  std::wstring username, append_path, path;
  path = GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::AppendToPath(&path, GetRegistryKey());
  file_util::AppendToPath(&path, installer_util::kInstallerDir);
  file_util::AppendToPath(&path,
      mini_installer_constants::kChromeSetupExecutable);
  printf("uninstall path is %ls\n", path.c_str());
  return path;
}

// Returns Chrome pv registry key value
std::wstring ChromeMiniInstaller::GetRegistryKey() {
  std::wstring build_key_value;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey key(GetRootRegistryKey(), dist->GetVersionKey().c_str());
  if (!key.ReadValue(L"pv", &build_key_value))
    return false;
  printf("Build key value is %ls\n", build_key_value.c_str());
  return build_key_value;
}

// Get HKEY based on install type.
HKEY ChromeMiniInstaller::GetRootRegistryKey() {
  HKEY type = HKEY_CURRENT_USER;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    type = HKEY_LOCAL_MACHINE;
  return type;
}

// This method checks if the build is Google Chrome or Chromium.
bool ChromeMiniInstaller::IsChromiumBuild() {
  #if defined(GOOGLE_CHROME_BUILD)
     return false;
  #else
     return true;
  #endif
}

// Launches the chrome installer and waits for it to end.
void ChromeMiniInstaller::LaunchInstaller(std::wstring path,
                                    const wchar_t process_name[]) {
  ASSERT_TRUE(file_util::PathExists(path));
  std::wstring launch_args;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    launch_args = L" -system-level";
  base::LaunchApp(L"\"" + path + L"\"" + launch_args, false, false, NULL);
  printf("Waiting while this process is running  %ls ....", process_name);
  WaitUntilProcessStartsRunning(process_name);
  WaitUntilProcessStopsRunning(process_name);
}

// Launch Chrome to see if it works after overinstall. Then close it.
void ChromeMiniInstaller::VerifyChromeLaunch() {
  std::wstring username, path, append_path;
  path = GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::AppendToPath(&path, installer_util::kChromeExe);
  printf("\n\nChrome is launched from %ls\n\n", path.c_str());
  base::LaunchApp(L"\"" + path + L"\"", false, false, NULL);
  WaitUntilProcessStartsRunning(installer_util::kChromeExe);
  Sleep(1200);
}

// This method compares the registry keys after overinstall.
bool ChromeMiniInstaller::VerifyOverInstall(
                              std::wstring value_before_overinstall,
                              std::wstring value_after_overinstall) {
  int64 reg_key_value_before_overinstall =  StringToInt64(
                                                 value_before_overinstall);
  int64 reg_key_value_after_overinstall =  StringToInt64(
                                                 value_after_overinstall);
  // Compare to see if the version is less.
  printf("Reg Key value before overinstall is%ls\n",
          value_before_overinstall.c_str());
  printf("Reg Key value after overinstall is%ls\n",
         value_after_overinstall.c_str());
  if (reg_key_value_before_overinstall > reg_key_value_after_overinstall) {
    printf("FAIL: Overinstalled a lower version of Chrome\n");
    return false;
  }
  return true;
}

// Waits until the process starts running.
void ChromeMiniInstaller::WaitUntilProcessStartsRunning(
                              const wchar_t process_name[]) {
  int timer = 0;
  while ((base::GetProcessCount(process_name, NULL) == 0) &&
         (timer < 60000)) {
    Sleep(200);
    timer = timer + 200;
  }
  ASSERT_NE(0, base::GetProcessCount(process_name, NULL));
}

// Waits until the process stops running.
void ChromeMiniInstaller::WaitUntilProcessStopsRunning(
                              const wchar_t process_name[]) {
  int timer = 0;
  printf("\nWaiting for this process to end... %ls\n", process_name);
  while ((base::GetProcessCount(process_name, NULL) > 0) &&
         (timer < 60000)) {
    Sleep(200);
    timer = timer + 200;
  }
  ASSERT_EQ(0, base::GetProcessCount(process_name, NULL));
}

