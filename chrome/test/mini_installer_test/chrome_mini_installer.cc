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
  // If need to do clean installation, uninstall chrome if exists.
  if (!over_install) {
    UnInstall();
  }
  LaunchExe(mini_installer_constants::kChromeMiniInstallerExecutable,
            mini_installer_constants::kChromeMiniInstallerExecutable);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey()));
  FindChromeShortcut();
  WaitUntilProcessStartsRunning(installer_util::kChromeExe);
  if (!over_install) {
    ASSERT_TRUE(CloseWindow(
        mini_installer_constants::kFirstChromeUI, WM_CLOSE));
  } else {
    ASSERT_TRUE(CloseWindow(
        mini_installer_constants::kBrowserTabName, WM_CLOSE));
  }
  VerifyChromeLaunch();
}

// Installs chromesetupdev.exe, waits for the install to finish and then
// checks the registry and shortcuts.
void ChromeMiniInstaller::InstallChromeSetupDev() {
  // Uninstall chrome, if already installed.
  UnInstall();

  // Install older/dev version of chrome.
  LaunchExe(mini_installer_constants::kChromeSetupDevExeLocation,
            mini_installer_constants::kChromeSetupExecutable);
  WaitUntilProcessStopsRunning(
      mini_installer_constants::kChromeMiniInstallerExecutable);
  std::wstring chrome_google_update_state_key(
      google_update::kRegPathClientState);
  chrome_google_update_state_key.append(L"\\");
  chrome_google_update_state_key.append(google_update::kChromeGuid);
  ASSERT_TRUE(CheckRegistryKey(chrome_google_update_state_key));
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey()));
  FindChromeShortcut();
  WaitUntilProcessStartsRunning(installer_util::kChromeExe);
  ASSERT_TRUE(CloseWindow(mini_installer_constants::kFirstChromeUI, WM_CLOSE));
}

// Accepts mini/dev as parameters. If mini, installs mini_installer first.
// if dev, installs ChromeSetupDev.exe first and
// then over installs with mini_installer.
// Also, verifies if Chrome can be launched sucessfully after overinstall.
void ChromeMiniInstaller::OverInstall() {
  InstallChromeSetupDev();
  // gets the registry key value before overinstall.
  std::wstring reg_key_value_returned = GetRegistryKey();
  printf("\n\nPreparing to overinstall...\n");
  printf("\nOverinstall path is %ls\n",
         mini_installer_constants::kChromeMiniInstallerExecutable);
  InstallMiniInstaller(true);
  // Get the registry key value after over install
  std::wstring reg_key_value_after_overinstall = GetRegistryKey();
  ASSERT_TRUE(VerifyOverInstall(
      reg_key_value_returned, reg_key_value_after_overinstall));
}

// This method first checks if Chrome is running.
// If yes, will close all the processes.
// Then will find and spawn uninstall path.
// Handles uninstall confirm dialog.
// Waits until setup.exe ends.
// Checks if registry key exist even after uninstall.
// Deletes App dir.
void ChromeMiniInstaller::UnInstall() {
  printf("Verifying if Chrome is installed...\n");
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!CheckRegistryKey(dist->GetVersionKey())) {
    printf("Chrome is not installed.\n");
    return;
  }

  printf("\nClosing Chrome processes, if any...\n");
  CloseChromeProcesses();

  std::wstring uninstall_path = GetUninstallPath();
  ASSERT_TRUE(file_util::PathExists(uninstall_path));

  printf("\nUninstalling Chrome...\n");
  process_util::LaunchApp(L"\"" + uninstall_path + L"\"" + L" -uninstall",
                          false, false, NULL);
  printf("\nLaunched setup.exe -uninstall....\n");
  ASSERT_TRUE(CloseWindow(
      mini_installer_constants::kConfirmDialog, WM_COMMAND));
  WaitUntilProcessStopsRunning(
      mini_installer_constants::kChromeSetupExecutable);
  ASSERT_FALSE(CheckRegistryKey(dist->GetVersionKey()));
  DeleteAppFolder();
  FindChromeShortcut();
  if (false == CloseWindow(mini_installer_constants::kChromeUninstallIETitle,
                           WM_CLOSE)) {
    printf("\nFailed to close window \"%s\".",
           mini_installer_constants::kChromeUninstallIETitle);
  }
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

// Checks for all running Chrome processes and kills them.
void ChromeMiniInstaller::CloseChromeProcesses() {
  int timer = 0;
  while ((process_util::GetProcessCount(installer_util::kChromeExe, NULL) > 0) &&
         (timer < 20000)) {
    process_util::KillProcesses(installer_util::kChromeExe, 1, NULL);
    Sleep(200);
    timer = timer + 200;
  }
  ASSERT_EQ(0, process_util::GetProcessCount(installer_util::kChromeExe, NULL));
}

// Checks for Chrome registry keys.
bool ChromeMiniInstaller::CheckRegistryKey(std::wstring key_path) {
  RegKey key;
  if (!key.Open(HKEY_CURRENT_USER,  key_path.c_str(),  KEY_ALL_ACCESS)) {
    printf("Cannot open reg key\n");
    return false;
  }
  std::wstring reg_key_value_returned = GetRegistryKey();
  printf("Reg key value is%ls\n",  reg_key_value_returned.c_str());
  return true;
}

// Deletes App folder after uninstall.
void ChromeMiniInstaller::DeleteAppFolder() {
  std::wstring path;
  ASSERT_TRUE(PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
  file_util::AppendToPath(&path, mini_installer_constants::kAppDir);
  file_util::UpOneDirectory(&path);
  printf("Deleting this path after uninstall%ls\n",  path.c_str());
  ASSERT_TRUE(file_util::Delete(path.c_str(), true));
}

// Verifies if Chrome shortcut exists.
void ChromeMiniInstaller::FindChromeShortcut() {
  std::wstring username, path_name, append_path, uninstall_lnk, shortcut_path;
  bool return_val = false;
  ASSERT_TRUE(PathService::Get(base::DIR_START_MENU, &path_name));
  file_util::AppendToPath(&path_name, L"Google Chrome");
  // Verify if path exists.
  if (file_util::PathExists(path_name)) {
    return_val = true;
    uninstall_lnk = path_name;
    file_util::AppendToPath(&path_name, L"Google Chrome.lnk");
    file_util::AppendToPath(&uninstall_lnk, L"Uninstall Google Chrome.lnk");
    ASSERT_TRUE(file_util::PathExists(path_name));
    ASSERT_TRUE(file_util::PathExists(uninstall_lnk));
  }
  if (return_val)
    printf("Chrome shortcuts found are:\n%ls\n%ls\n",
           path_name.c_str(), uninstall_lnk.c_str());
  else
    printf("Chrome shortcuts not found\n");
}

// Gets the path for uninstall.
std::wstring ChromeMiniInstaller::GetUninstallPath() {
  std::wstring username, append_path, path;
  std::wstring build_key_value = GetRegistryKey();
  PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
  file_util::AppendToPath(&path, mini_installer_constants::kAppDir);
  file_util::AppendToPath(&path, build_key_value);
  file_util::AppendToPath(&path, installer::kInstallerDir);
  file_util::AppendToPath(&path,
      mini_installer_constants::kChromeSetupExecutable);
  printf("uninstall path is %ls\n", path.c_str());
  return path;
}

// Reads Chrome registry key.
std::wstring ChromeMiniInstaller::GetRegistryKey() {
  std::wstring build_key_value;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey key(HKEY_CURRENT_USER, dist->GetVersionKey().c_str());
  if (!key.ReadValue(L"pv", &build_key_value))
    return false;
  return build_key_value;
}

// Launches a given executable and waits until it is done.
void ChromeMiniInstaller::LaunchExe(std::wstring path,
                                    const wchar_t process_name[]) {
  printf("\nBuild to be installed is:   %ls\n", path.c_str());
  ASSERT_TRUE(file_util::PathExists(path));
  process_util::LaunchApp(L"\"" + path + L"\"", false, false, NULL);
  printf("Waiting while this process is running  %ls ....", process_name);
  WaitUntilProcessStartsRunning(process_name);
  WaitUntilProcessStopsRunning(process_name);
}

// Launch Chrome to see if it works after overinstall. Then close it.
void ChromeMiniInstaller::VerifyChromeLaunch() {
  std::wstring username, path, append_path;
  ASSERT_TRUE(PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
  file_util::AppendToPath(&path,
      mini_installer_constants::kAppDir);
  file_util::AppendToPath(&path, installer_util::kChromeExe);
  process_util::LaunchApp(L"\"" + path + L"\"", false, false, NULL);
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
  while ((process_util::GetProcessCount(process_name, NULL) == 0) &&
         (timer < 60000)) {
    Sleep(200);
    timer = timer + 200;
  }
  ASSERT_NE(0, process_util::GetProcessCount(process_name, NULL));
}

// Waits until the process stops running.
void ChromeMiniInstaller::WaitUntilProcessStopsRunning(
                              const wchar_t process_name[]) {
  int timer = 0;
  printf("\nWaiting for this process to end... %ls\n", process_name);
  while ((process_util::GetProcessCount(process_name, NULL) > 0) &&
         (timer < 60000)) {
    Sleep(200);
    timer = timer + 200;
  }
  ASSERT_EQ(0, process_util::GetProcessCount(process_name, NULL));
}
