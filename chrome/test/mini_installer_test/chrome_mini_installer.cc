// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/chrome_mini_installer.h"

#include <algorithm>

#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// This method will get the previous latest full installer from
// nightly location, install it and over install with diff installer.
void ChromeMiniInstaller::InstallDifferentialInstaller() {
  std::wstring diff_installer;
  ASSERT_TRUE(GetInstaller(mini_installer_constants::kDiffInstallerPattern,
                           &diff_installer));
  printf("\nLatest differential installer name is %ls\n",
         diff_installer.c_str());
  std::wstring prev_full_installer;
  ASSERT_TRUE(GetPreviousFullInstaller(diff_installer, &prev_full_installer));
  printf("\nPrevious full installer name is %ls\n",
         prev_full_installer.c_str());
  InstallMiniInstaller(false, prev_full_installer);
  std::wstring full_installer_value;
  GetChromeVersionFromRegistry(&full_installer_value);
  printf("\n\nPreparing to overinstall...\n");
  InstallMiniInstaller(false, diff_installer);
  std::wstring diff_installer_value;
  GetChromeVersionFromRegistry(&diff_installer_value);
  ASSERT_TRUE(VerifyDifferentialInstall(full_installer_value,
                                        diff_installer_value, diff_installer));
}

// This method will get the diff installer file name and
// then derives the previous and latest build numbers.
bool ChromeMiniInstaller::VerifyDifferentialInstall(
    const std::wstring& full_installer_value,
    const std::wstring& diff_installer_value,
    const std::wstring& diff_path) {
  std::wstring diff_installer_name = file_util::GetFilenameFromPath(diff_path);
  std::wstring actual_full_installer_value;
  actual_full_installer_value.assign(
     mini_installer_constants::kDevChannelBuildPattern);
  // This substring will give the full installer build number.
  actual_full_installer_value.append(diff_installer_name.substr(11, 5));
  std::wstring actual_diff_installer_value;
  actual_diff_installer_value.assign(
     mini_installer_constants::kDevChannelBuildPattern);
  // This substring will give the diff installer build number.
  actual_diff_installer_value.append(diff_installer_name.substr(0, 5));
  if ((actual_full_installer_value == full_installer_value) &&
      (actual_diff_installer_value == diff_installer_value)) {
    return true;
  } else {
    printf("\n The diff installer is not successful. Here are the values:\n");
    printf("\n Expected full installer value: %ls and actual value is %ls\n",
           full_installer_value.c_str(), actual_full_installer_value.c_str());
    printf("\n Expected diff installer value: %ls and actual value is %ls\n",
           diff_installer_value.c_str(), actual_diff_installer_value.c_str());
    return false;
  }
}

// This method will get the latest full installer from nightly location
// and installs it.
void ChromeMiniInstaller::InstallFullInstaller() {
  std::wstring full_installer_file_name;
  ASSERT_TRUE(GetInstaller(mini_installer_constants::kFullInstallerPattern,
                           &full_installer_file_name));
  printf("The latest full installer is %ls\n\n",
         full_installer_file_name.c_str());
  InstallMiniInstaller(false, full_installer_file_name);
}

// Installs the Chrome mini-installer, checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMiniInstaller(bool over_install,
                                               const std::wstring& path) {
  std::wstring exe_name = file_util::GetFilenameFromPath(path);
  printf("\nChrome will be installed at %ls level\n", install_type_.c_str());
  printf("\nWill proceed with the test only if this path exists: %ls\n\n",
         path.c_str());
  ASSERT_TRUE(file_util::PathExists(path));
  LaunchInstaller(path, exe_name.c_str());
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey()));
  printf("\nInstall Checks:\n\n");
  FindChromeShortcut();
  VerifyChromeLaunch();
  CloseFirstRunUIDialog(over_install);
}

// This method tests the standalone installer by verifying the steps listed at:
// https://sites.google.com/a/google.com/chrome-pmo/
// standalone-installers/testing-standalone-installers
// This method applies appropriate tags to standalone installer and deletes
// old installer before running the new tagged installer. It also verifies
// that the installed version is correct.
void ChromeMiniInstaller::InstallStandaloneIntaller() {
  if (!IsChromiumBuild()) {
    standalone_installer = true;
    file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
    std::wstring tag_installer_command;
    ASSERT_TRUE(GetCommandForTagging(&tag_installer_command));
    base::LaunchApp(tag_installer_command, true, false, NULL);
    std::wstring installer_path = GetInstallerExePath(
        mini_installer_constants::kStandaloneInstaller);
    InstallMiniInstaller(false, installer_path);
    ASSERT_TRUE(VerifyStandaloneInstall());
    file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
  } else {
    printf("\n\nThis test doesn't run on a chromium build\n");
  }
}

// Installs chromesetup.exe, waits for the install to finish and then
// checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMetaInstaller() {
  // Install Google Chrome through meta installer.
  LaunchInstaller(mini_installer_constants::kChromeMetaInstallerExe,
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
    std::wstring reg_key_value_returned;
    // gets the registry key value before overinstall.
    GetChromeVersionFromRegistry(&reg_key_value_returned);
    printf("\n\nPreparing to overinstall...\n");
    std::wstring installer_path = GetInstallerExePath(
        mini_installer_constants::kChromeMiniInstallerExecutable);
    InstallMiniInstaller(true, installer_path);
    std::wstring reg_key_value_after_overinstall;
    // Get the registry key value after over install
    GetChromeVersionFromRegistry(&reg_key_value_after_overinstall);
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
  printf("\n\nUninstall Checks:\n\n");
  ASSERT_FALSE(CheckRegistryKey(dist->GetVersionKey()));
  DeleteAppFolder();
  FindChromeShortcut();
  CloseProcesses(mini_installer_constants::kIEExecutable);
  ASSERT_EQ(0,
      base::GetProcessCount(mini_installer_constants::kIEExecutable, NULL));
}

// Takes care of Chrome uninstall dialog.
bool ChromeMiniInstaller::CloseWindow(const wchar_t* window_name,
                                      UINT message) {
  int timer = 0;
  bool return_val = false;
  HWND hndl = FindWindow(NULL, window_name);
  while (hndl == NULL && (timer < 60000)) {
    hndl = FindWindow(NULL, window_name);
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  if (hndl != NULL) {
    LRESULT _result = SendMessage(hndl, message, 1, 0);
    return_val = true;
  }
  return return_val;
}

// Closes Chrome browser.
void ChromeMiniInstaller::CloseChromeBrowser(const wchar_t* window_name) {
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
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  ASSERT_EQ(0, base::GetProcessCount(executable_name, NULL));
}

// Checks for Chrome registry keys.
bool ChromeMiniInstaller::CheckRegistryKey(const std::wstring& key_path) {
  RegKey key;
  if (!key.Open(GetRootRegistryKey(), key_path.c_str(), KEY_ALL_ACCESS)) {
    printf("Cannot open reg key\n");
    return false;
  }
  std::wstring reg_key_value_returned;
  if (!GetChromeVersionFromRegistry(&reg_key_value_returned))
    return false;
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

// This method will create a command line  to run apply tag.
bool ChromeMiniInstaller::GetCommandForTagging(std::wstring *return_command) {
  file_info_list file_details;
  ChromeMiniInstaller::GetStandaloneInstallerFileName(&file_details);
  std::wstring standalone_installer_name, standalone_installer_path;
  if (file_details.empty())
    return false;
  if (file_details.at(0).name_.empty())
    return false;
  standalone_installer_name.append(file_details.at(0).name_);
  standalone_installer_path.assign(
     mini_installer_constants::kChromeStandAloneInstallerLocation);
  standalone_installer_path.append(standalone_installer_name);
  return_command->append(
     mini_installer_constants::kChromeApplyTagExe);
  return_command->append(L" ");
  return_command->append(standalone_installer_path);
  return_command->append(L" ");
  return_command->append(mini_installer_constants::kStandaloneInstaller);
  return_command->append(L" ");
  return_command->append(mini_installer_constants::kChromeApplyTagParameters);
  printf("Command to run Apply tag is %ls\n", return_command->c_str());
  return true;
}

// Get path for mini_installer.exe.
std::wstring ChromeMiniInstaller::GetInstallerExePath(const wchar_t* name) {
  std::wstring installer_path;
  PathService::Get(base::DIR_EXE, &installer_path);
  file_util::AppendToPath(&installer_path, name);
  printf("Chrome exe path is %ls\n", installer_path.c_str());
  return installer_path;
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

// This is a predicate to sort file_info.
bool IsNewer(const file_info& file_rbegin, const file_info& file_rend) {
  return (file_rbegin.creation_time_ > file_rend.creation_time_);
}

// This method will first call GetLatestFile to get the list of all
// builds, sorted on creation time. Then goes through each build folder
// until it finds the installer file that matches the pattern argument.
bool ChromeMiniInstaller::GetInstaller(const wchar_t* pattern,
                                       std::wstring *path) {
  file_info_list list;
  std::wstring chrome_diff_installer(
     mini_installer_constants::kChromeDiffInstallerLocation);
  chrome_diff_installer.append(L"*");
  if (!GetLatestFile(chrome_diff_installer.c_str(),
                mini_installer_constants::kDevChannelBuildPattern, &list))
    return false;
  int list_size = (list.size())-1;
  while (list_size > 0) {
    path->assign(L"");
    path->append(mini_installer_constants::kChromeDiffInstallerLocation);
    file_util::AppendToPath(path, list.at(list_size).name_.c_str());
    std::wstring build_number;
    build_number.assign(list.at(list_size).name_.c_str());
    std::wstring installer_path(
       mini_installer_constants::kChromeDiffInstallerLocation);
    installer_path.append(build_number.c_str());
    file_util::AppendToPath(&installer_path, L"*.exe");
    if (!GetLatestFile(installer_path.c_str(), pattern, &list)) {
      list_size--;
    } else {
      file_util::AppendToPath(path, list.at(0).name_.c_str());
      if (!file_util::PathExists(*path)) {
        list_size--;
      } else {
        break;
      }
    }
  }
  return (file_util::PathExists(path->c_str()));
}

// This method will get the latest installer filename from the directory.
bool ChromeMiniInstaller::GetLatestFile(const wchar_t* file_name,
                                        const wchar_t* pattern,
                                        file_info_list *file_details) {
  WIN32_FIND_DATA find_file_data;
  HANDLE file_handle = FindFirstFile(file_name, &find_file_data);
  if (file_handle == INVALID_HANDLE_VALUE) {
    printf("Handle is invalid\n");
    return false;
  }
  BOOL ret = TRUE;
  while (ret) {
    std::wstring search_path = find_file_data.cFileName;
    size_t position_found = search_path.find(pattern);
    if (position_found != -1) {
      std::wstring extension = file_util::GetFileExtensionFromPath(file_name);
      if (extension.compare(L"exe") == 0) {
        file_details->push_back(file_info(find_file_data.cFileName, 0));
        break;
      } else {
        FILETIME file_time = find_file_data.ftCreationTime;
        base::Time creation_time = base::Time::FromFileTime(file_time);
        file_details->push_back(file_info(find_file_data.cFileName,
                                static_cast<int>(creation_time.ToDoubleT())));
      }
    }
    ret = FindNextFile(file_handle, &find_file_data);
  }
  std::sort(file_details->rbegin(), file_details->rend(), &IsNewer);
  FindClose(file_handle);
  return true;
}

// This method will get the previous full installer path
// from given diff installer path. It will first get the
// filename from the diff installer path, gets the previous
// build information from the filename, then computes the
// path for previous full installer.
bool ChromeMiniInstaller::GetPreviousFullInstaller(
    const std::wstring& diff_file_name, std::wstring *previous) {
  std::wstring diff_file = diff_file_name;
  std::wstring name = file_util::GetFilenameFromPath(diff_file);
  file_util::UpOneDirectory(&diff_file);
  file_util::UpOneDirectory(&diff_file);
  std::wstring file_name = name.substr(11, 15);
  std::wstring::size_type last_dot = file_name.find(L'_');
  file_name = file_name.substr(0, last_dot);
  file_name = L"2.0." + file_name;
  file_util::AppendToPath(&diff_file, file_name.c_str());
  previous->assign(diff_file);
  file_util::AppendToPath(&diff_file, L"*.exe");
  file_info_list directory_list;
  if (!GetLatestFile(diff_file.c_str(),
                mini_installer_constants::kFullInstallerPattern,
                &directory_list))
    return false;
  file_util::AppendToPath(previous, directory_list.at(0).name_);
  return (file_util::PathExists(previous->c_str()));
}

// This method will return standalone installer file name.
bool ChromeMiniInstaller::GetStandaloneInstallerFileName(
    file_info_list *file_name) {
  std::wstring standalone_installer(
     mini_installer_constants::kChromeStandAloneInstallerLocation);
  standalone_installer.append(L"*.exe");
  return (GetLatestFile(standalone_installer.c_str(),
                        mini_installer_constants::kUntaggedInstallerPattern,
                        file_name));
}

// This method will get the version number from the filename.
bool ChromeMiniInstaller::GetStandaloneVersion(std::wstring* return_file_name) {
  file_info_list file_details;
  ChromeMiniInstaller::GetStandaloneInstallerFileName(&file_details);
  std::wstring file_name = file_details.at(0).name_;
  // Returned file name will have following convention:
  // ChromeStandaloneSetup_<build>_<patch>.exe
  // Following code will extract build, patch details from the file
  // and concatenate with 1.0 to form the build version.
  // Patteren followed: 1.0.<build>.<patch>htt
  file_name = file_name.substr(22, 25);
  std::wstring::size_type last_dot = file_name.find(L'.');
  file_name = file_name.substr(0, last_dot);
  std::wstring::size_type pos = file_name.find(L'_');
  file_name.replace(pos, 1, L".");
  file_name = L"1.0." + file_name;
  return_file_name->assign(file_name.c_str());
  printf("Standalone installer version is %ls\n", file_name.c_str());
  return true;
}

// Gets the path for uninstall.
std::wstring ChromeMiniInstaller::GetUninstallPath() {
  std::wstring username, append_path, path, reg_key_value;
  GetChromeVersionFromRegistry(&reg_key_value);
  path = GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::AppendToPath(&path, reg_key_value);
  file_util::AppendToPath(&path, installer_util::kInstallerDir);
  file_util::AppendToPath(&path,
      mini_installer_constants::kChromeSetupExecutable);
  printf("uninstall path is %ls\n", path.c_str());
  return path;
}

// Returns Chrome pv registry key value
bool ChromeMiniInstaller::GetChromeVersionFromRegistry(
    std::wstring* build_key_value ) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey key(GetRootRegistryKey(), dist->GetVersionKey().c_str());
  if (!key.ReadValue(L"pv", build_key_value)) {
    printf("registry key not found\n");
    return false;
  }
  printf("Build key value is %ls\n\n", build_key_value->c_str());
  return true;
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
void ChromeMiniInstaller::LaunchInstaller(const std::wstring& path,
                                          const wchar_t* process_name) {
  ASSERT_TRUE(file_util::PathExists(path));
  if (install_type_ == mini_installer_constants::kSystemInstall) {
    std::wstring launch_args = L" -system-level";
    base::LaunchApp(L"\"" + path + L"\"" + launch_args, false, false, NULL);
  } else {
    base::LaunchApp(L"\"" + path + L"\"", false, false, NULL);
  }
  printf("Waiting while this process is running  %ls ....\n", process_name);
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
}

// This method compares the registry keys after overinstall.
bool ChromeMiniInstaller::VerifyOverInstall(
    const std::wstring& value_before_overinstall,
    const std::wstring& value_after_overinstall) {
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

// This method will verify if the installed build is correct.
bool ChromeMiniInstaller::VerifyStandaloneInstall() {
  std::wstring reg_key_value_returned, standalone_installer_version;
  GetStandaloneVersion(&standalone_installer_version);
  GetChromeVersionFromRegistry(&reg_key_value_returned);
  if (standalone_installer_version.compare(reg_key_value_returned) == 0)
    return true;
  else
    return false;
}
// Waits until the process starts running.
void ChromeMiniInstaller::WaitUntilProcessStartsRunning(
    const wchar_t* process_name) {
  int timer = 0;
  while ((base::GetProcessCount(process_name, NULL) == 0) &&
         (timer < 60000)) {
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  ASSERT_NE(0, base::GetProcessCount(process_name, NULL));
}

// Waits until the process stops running.
void ChromeMiniInstaller::WaitUntilProcessStopsRunning(
    const wchar_t* process_name) {
  int timer = 0;
  printf("\nWaiting for this process to end... %ls\n", process_name);
  while ((base::GetProcessCount(process_name, NULL) > 0) &&
         (timer < 60000)) {
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  ASSERT_EQ(0, base::GetProcessCount(process_name, NULL));
}
