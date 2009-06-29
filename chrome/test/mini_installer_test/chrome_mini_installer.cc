// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/chrome_mini_installer.h"

#include <algorithm>

#include "base/file_util.h"
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

// Installs Chrome.
void ChromeMiniInstaller::Install() {
  std::wstring installer_path = GetFilePath(
        mini_installer_constants::kChromeMiniInstallerExecutable);
    InstallMiniInstaller(false, installer_path);
}

// This method will get the previous latest full installer from
// nightly location, install it and over install with specified install_type.
void ChromeMiniInstaller::OverInstallOnFullInstaller(
    const std::wstring& install_type) {
  std::wstring diff_installer;
  ASSERT_TRUE(GetInstaller(mini_installer_constants::kDiffInstallerPattern,
                           &diff_installer));
  std::wstring prev_full_installer;
  ASSERT_TRUE(GetPreviousFullInstaller(diff_installer, &prev_full_installer));
  printf("\nPrevious full installer name is %ls\n",
         prev_full_installer.c_str());
  InstallMiniInstaller(false, prev_full_installer);
  std::wstring full_installer_value;
  GetChromeVersionFromRegistry(&full_installer_value);
  printf("\n\nPreparing to overinstall...\n");
  if (install_type == mini_installer_constants::kDiffInstall) {
    printf("\nOver installing with latest differential installer: %ls\n",
           diff_installer.c_str());
    InstallMiniInstaller(true, diff_installer);
  } else if (install_type == mini_installer_constants::kFullInstall) {
    std::wstring latest_full_installer;
    ASSERT_TRUE(GetInstaller(mini_installer_constants::kFullInstallerPattern,
                             &latest_full_installer));
    printf("\nOver installing with latest full insatller: %ls\n",
           latest_full_installer.c_str());
    InstallMiniInstaller(true, latest_full_installer);
  }
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
  std::wstring actual_full_installer_value;
  std::wstring diff_installer_name = file_util::GetFilenameFromPath(diff_path);
  GetPreviousBuildNumber(diff_path, &actual_full_installer_value);
  // This substring will give the full installer build number.
  std::wstring actual_diff_installer_value;
  actual_diff_installer_value.assign(build_channel_);
  // This substring will give the diff installer build number.
  actual_diff_installer_value.append(diff_installer_name.substr(0,
                                     diff_installer_name.find(L'_')));
  if ((actual_full_installer_value == full_installer_value) &&
      (actual_diff_installer_value == diff_installer_value)) {
    printf("\n The diff installer is successful. Here are the values:\n");
    printf("\n full installer value: %ls and diff installer value is %ls\n",
           full_installer_value.c_str(), diff_installer_value.c_str());
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

// This method will get previous build number.
void ChromeMiniInstaller::GetPreviousBuildNumber(
    const std::wstring& path, std::wstring *build_number) {
  std::wstring diff_installer_name = file_util::GetFilenameFromPath(path);
  std::wstring::size_type start_position = diff_installer_name.find(L"f");
  std::wstring::size_type end_position = diff_installer_name.find(L"_c");
  end_position = end_position - start_position;
  std::wstring file_name = diff_installer_name.substr(start_position,
                                                      end_position);
  file_name = file_name.substr(file_name.find(L'_')+1, file_name.size());
  file_name = build_channel_ + file_name;
  build_number->assign(file_name);
  printf("Previous build number is %ls\n", file_name.c_str());
}

// This method will get the latest full installer from nightly location
// and installs it.
void ChromeMiniInstaller::InstallFullInstaller(bool over_install) {
  std::wstring full_installer_file_name;
  ASSERT_TRUE(GetInstaller(mini_installer_constants::kFullInstallerPattern,
                           &full_installer_file_name));
  printf("The latest full installer is %ls\n\n",
         full_installer_file_name.c_str());
  InstallMiniInstaller(over_install, full_installer_file_name);
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
  if ((install_type_ == mini_installer_constants::kUserInstall) &&
      (!over_install))
    CloseFirstRunUIDialog(over_install);
  PlatformThread::Sleep(800);
  FindChromeShortcut();
  LaunchAndCloseChrome(over_install);
}

// This method tests the standalone installer by verifying the steps listed at:
// https://sites.google.com/a/google.com/chrome-pmo/
// standalone-installers/testing-standalone-installers
// This method applies appropriate tags to standalone installer and deletes
// old installer before running the new tagged installer. It also verifies
// that the installed version is correct.
void ChromeMiniInstaller::InstallStandaloneInstaller() {
  standalone_installer = true;
  file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
  std::wstring tag_installer_command;
  ASSERT_TRUE(GetCommandForTagging(&tag_installer_command));
  base::LaunchApp(tag_installer_command, true, false, NULL);
  std::wstring installer_path = GetFilePath(
      mini_installer_constants::kStandaloneInstaller);
  InstallMiniInstaller(false, installer_path);
  ASSERT_TRUE(VerifyStandaloneInstall());
  file_util::Delete(mini_installer_constants::kStandaloneInstaller, true);
}

// Installs chromesetup.exe, waits for the install to finish and then
// checks the registry and shortcuts.
void ChromeMiniInstaller::InstallMetaInstaller() {
  // Install Google Chrome through meta installer.
  LaunchInstaller(mini_installer_constants::kChromeMetaInstallerExe,
                  mini_installer_constants::kChromeSetupExecutable);
  VerifyProcessClose(mini_installer_constants::kChromeMetaInstallerExecutable);
  std::wstring chrome_google_update_state_key(
      google_update::kRegPathClients);
  chrome_google_update_state_key.append(L"\\");
  chrome_google_update_state_key.append(google_update::kChromeGuid);
  ASSERT_TRUE(CheckRegistryKey(chrome_google_update_state_key));
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ASSERT_TRUE(CheckRegistryKey(dist->GetVersionKey()));
  FindChromeShortcut();
  LaunchAndCloseChrome(false);
}

// If the build type is Google Chrome, then it first installs meta installer
// and then over installs with mini_installer. It also verifies if Chrome can
// be launched successfully after overinstall.
void ChromeMiniInstaller::OverInstall() {
  InstallMetaInstaller();
  std::wstring reg_key_value_returned;
  // gets the registry key value before overinstall.
  GetChromeVersionFromRegistry(&reg_key_value_returned);
  printf("\n\nPreparing to overinstall...\n");
  InstallFullInstaller(true);
  std::wstring reg_key_value_after_overinstall;
  // Get the registry key value after over install
  GetChromeVersionFromRegistry(&reg_key_value_after_overinstall);
  ASSERT_TRUE(VerifyOverInstall(reg_key_value_returned,
                                reg_key_value_after_overinstall));
}

// This method will first install Chrome. Deletes either registry or
// folder based on the passed argument, then tries to launch Chrome.
// Then installs Chrome again to repair.
void ChromeMiniInstaller::Repair(
    ChromeMiniInstaller::RepairChrome repair_type) {
  InstallFullInstaller(false);
  CloseProcesses(installer_util::kChromeExe);
  if (repair_type == ChromeMiniInstaller::VERSION_FOLDER) {
    DeleteFolder(L"version_folder");
    printf("Deleted folder. Now trying to launch chrome\n");
  } else if (repair_type == ChromeMiniInstaller::REGISTRY) {
    DeletePvRegistryKey();
    printf("Deleted registry. Now trying to launch chrome\n");
  }
  std::wstring current_path;
  ChangeCurrentDirectory(&current_path);
  VerifyChromeLaunch(false);
  printf("\nInstalling Chrome again to see if it can be repaired\n\n");
  InstallFullInstaller(true);
  printf("Chrome repair successful.\n");
  // Set the current directory back to original path.
  ::SetCurrentDirectory(current_path.c_str());
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
  if (uninstall_path == L"") {
    printf("\n Chrome install is in a weird state. Cleaning the machine...\n");
    CleanChromeInstall();
    return;
  }
  ASSERT_TRUE(file_util::PathExists(uninstall_path));
  std::wstring uninstall_args = L"\"" + uninstall_path +
                                L"\"" + L" --uninstall --force-uninstall";
  if (install_type_ == mini_installer_constants::kSystemInstall)
    uninstall_args = uninstall_args + L" --system-level";
  base::LaunchApp(uninstall_args, false, false, NULL);
  printf("Launched setup.exe. Here are the commands passed: %ls\n",
         uninstall_args.c_str());
  // ASSERT_TRUE(CloseUninstallWindow());
  VerifyProcessClose(mini_installer_constants::kChromeSetupExecutable);
  printf("\n\nUninstall Checks:\n\n");
  ASSERT_FALSE(CheckRegistryKeyOnUninstall(dist->GetVersionKey()));
  printf("Deleting user data folder after uninstall\n");
  DeleteUserDataFolder();
  FindChromeShortcut();
  CloseProcesses(mini_installer_constants::kIEExecutable);
  ASSERT_EQ(0,
      base::GetProcessCount(mini_installer_constants::kIEExecutable, NULL));
}

// Will clean up the machine if Chrome install is messed up.
void ChromeMiniInstaller::CleanChromeInstall() {
  DeletePvRegistryKey();
  DeleteFolder(mini_installer_constants::kChromeAppDir);
}

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

// This method will send enter key.
void ChromeMiniInstaller::SendEnterKeyToWindow() {
  INPUT key;
  key.type = INPUT_KEYBOARD;
  key.ki.wVk = VK_RETURN;
  key.ki.dwFlags = 0;
  key.ki.time = 0;
  key.ki.wScan = 0;
  key.ki.dwExtraInfo = 0;
  SendInput(1, &key, sizeof(INPUT));
  key.ki.dwExtraInfo = KEYEVENTF_KEYUP;
  SendInput(1, &key, sizeof(INPUT));
}

bool ChromeMiniInstaller::CloseUninstallWindow() {
  HWND hndl = NULL;
  int timer = 0;
  while (hndl == NULL && timer < 5000) {
    hndl = FindWindow(NULL,
        mini_installer_constants::kChromeUninstallDialogName);
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }

  if (hndl == NULL)
    hndl = FindWindow(NULL, mini_installer_constants::kChromeBuildType);

  if (hndl == NULL)
    return false;

  SetForegroundWindow(hndl);
  SendEnterKeyToWindow();
  return true;
}

// Closes Chrome browser.
bool ChromeMiniInstaller::CloseChromeBrowser() {
  int timer = 0;
  HWND handle = NULL;
  // This loop iterates through all of the top-level Windows
  // named Chrome_WidgetWin_0 and closes them
  while ((base::GetProcessCount(installer_util::kChromeExe, NULL) > 0) &&
         (timer < 40000)) {
    // Chrome may have been launched, but the window may not have appeared
    // yet. Wait for it to appear for 10 seconds, but exit if it takes longer
    // than that.
    while (!handle && timer < 10000) {
      handle = FindWindowEx(NULL, handle, L"Chrome_WidgetWin_0", NULL);
      if (!handle) {
        PlatformThread::Sleep(100);
        timer = timer + 100;
      }
    }
    if (!handle)
      return false;
    SetForegroundWindow(handle);
    LRESULT _result = SendMessage(handle, WM_CLOSE, 1, 0);
    if (_result != 0)
      return false;
    PlatformThread::Sleep(1000);
    timer = timer + 1000;
  }
  if (base::GetProcessCount(installer_util::kChromeExe, NULL) > 0) {
    printf("Chrome.exe is still running even after closing all windows\n");
    return false;
  }
  return true;
}

// Closes the First Run UI dialog.
void ChromeMiniInstaller::CloseFirstRunUIDialog(bool over_install) {
  VerifyProcessLaunch(installer_util::kChromeExe, true);
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

// Change current directory so that chrome.dll from current folder
// will not be used as fall back.
void ChromeMiniInstaller::ChangeCurrentDirectory(std::wstring *current_path) {
  wchar_t backup_path[MAX_PATH];
  DWORD ret = ::GetCurrentDirectory(MAX_PATH, backup_path);
  ASSERT_TRUE(0 != ret && ret < MAX_PATH);
  current_path->assign(backup_path);
  file_util::UpOneDirectory(current_path);
  ::SetCurrentDirectory(current_path->c_str());
  current_path->assign(backup_path);
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

// Checks for Chrome registry keys on uninstall.
bool ChromeMiniInstaller::CheckRegistryKeyOnUninstall(
    const std::wstring& key_path) {
  RegKey key;
  int timer = 0;
  while ((key.Open(GetRootRegistryKey(), key_path.c_str(), KEY_ALL_ACCESS)) &&
         (timer < 20000)) {
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  return CheckRegistryKey(key_path);
}

// Deletes Installer folder from Applications directory.
void ChromeMiniInstaller::DeleteFolder(const wchar_t* folder_name) {
  std::wstring install_path = GetChromeInstallDirectoryLocation();
  if (folder_name == L"version_folder") {
    std::wstring delete_path;
    delete_path = mini_installer_constants::kChromeAppDir;
    std::wstring build_number;
    GetChromeVersionFromRegistry(&build_number);
    delete_path = delete_path + build_number;
    file_util::AppendToPath(&install_path, delete_path);
  } else if (folder_name == mini_installer_constants::kChromeAppDir) {
    file_util::AppendToPath(&install_path, folder_name);
    file_util::TrimTrailingSeparator(&install_path);
  }
  printf("This path will be deleted: %ls\n", install_path.c_str());
  ASSERT_TRUE(file_util::Delete(install_path.c_str(), true));
}

// Will delete user data profile.
void ChromeMiniInstaller::DeleteUserDataFolder() {
  FilePath path;
  PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
  std::wstring profile_path = path.ToWStringHack();
  file_util::AppendToPath(&profile_path,
                          mini_installer_constants::kChromeAppDir);
  file_util::UpOneDirectory(&profile_path);
  file_util::AppendToPath(&profile_path,
                          mini_installer_constants::kChromeUserDataDir);
  printf("\nDeleting this path after uninstall%ls\n", profile_path.c_str());
  if (file_util::PathExists(path))
    ASSERT_TRUE(file_util::Delete(profile_path.c_str(), true));
}

// Deletes pv key from Clients.
void ChromeMiniInstaller::DeletePvRegistryKey() {
  std::wstring pv_key(google_update::kRegPathClients);
  pv_key.append(L"\\");
  pv_key.append(google_update::kChromeGuid);
  RegKey key;
  if (key.Open(GetRootRegistryKey(), pv_key.c_str(), KEY_ALL_ACCESS))
    ASSERT_TRUE(key.DeleteValue(L"pv"));
  printf("Deleted %ls key\n", pv_key.c_str());
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
  FilePath path;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    PathService::Get(base::DIR_PROGRAM_FILES, &path);
  else
    PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
  return path.ToWStringHack();
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

// Gets path for the specified parameter.
std::wstring ChromeMiniInstaller::GetFilePath(const wchar_t* name) {
  FilePath installer_path;
  PathService::Get(base::DIR_EXE, &installer_path);
  installer_path = installer_path.Append(name);
  printf("Chrome exe path is %ls\n", installer_path.value().c_str());
  return installer_path.ToWStringHack();
}

// This method gets the shortcut path  from startmenu based on install type
std::wstring ChromeMiniInstaller::GetStartMenuShortcutPath() {
  FilePath path_name;
  if (install_type_ == mini_installer_constants::kSystemInstall)
    PathService::Get(base::DIR_COMMON_START_MENU, &path_name);
  else
    PathService::Get(base::DIR_START_MENU, &path_name);
  return path_name.ToWStringHack();
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
                     build_channel_.c_str(), &list))
    return false;
  int list_size = (list.size())-1;
  while (list_size > 0) {
    path->assign(L"");
    path->append(mini_installer_constants::kChromeDiffInstallerLocation);
    file_util::AppendToPath(path, list.at(list_size).name_.c_str());
    if (build_channel_ == mini_installer_constants::kDevChannelBuild)
      file_util::AppendToPath(path, L"win");
    std::wstring build_number;
    build_number.assign(list.at(list_size).name_.c_str());
    std::wstring installer_path(
       mini_installer_constants::kChromeDiffInstallerLocation);
    installer_path.append(build_number.c_str());
    if (build_channel_ == mini_installer_constants::kDevChannelBuild)
      file_util::AppendToPath(&installer_path, L"win");
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
  std::wstring build_number;
  GetPreviousBuildNumber(diff_file, &build_number);
  file_util::UpOneDirectory(&diff_file);
  file_util::UpOneDirectory(&diff_file);
  if (build_channel_ == mini_installer_constants::kDevChannelBuild)
    file_util::UpOneDirectory(&diff_file);
  file_util::AppendToPath(&diff_file, build_number.c_str());
  if (build_channel_ == mini_installer_constants::kDevChannelBuild)
    file_util::AppendToPath(&diff_file, L"win");
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
  file_name = L"2.0." + file_name;
  return_file_name->assign(file_name.c_str());
  printf("Standalone installer version is %ls\n", file_name.c_str());
  return true;
}

// Gets the path for uninstall.
std::wstring ChromeMiniInstaller::GetUninstallPath() {
  std::wstring username, append_path, path, reg_key_value;
  if (!GetChromeVersionFromRegistry(&reg_key_value))
    return L"";
  path = GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::AppendToPath(&path, reg_key_value);
  file_util::AppendToPath(&path, installer_util::kInstallerDir);
  file_util::AppendToPath(&path,
      mini_installer_constants::kChromeSetupExecutable);
  if (!file_util::PathExists(path)) {
    printf("This uninstall path is not correct %ls. Will not proceed further",
           path.c_str());
    return L"";
  }
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
  VerifyProcessLaunch(process_name, true);
  VerifyProcessClose(process_name);
}

// Gets the path to launch Chrome.
bool ChromeMiniInstaller::GetChromeLaunchPath(std::wstring* launch_path) {
  std::wstring path;
  path = GetChromeInstallDirectoryLocation();
  file_util::AppendToPath(&path, mini_installer_constants::kChromeAppDir);
  file_util::AppendToPath(&path, installer_util::kChromeExe);
  launch_path->assign(path);
  return(file_util::PathExists(path));
}

// Launch Chrome to see if it works after overinstall. Then close it.
void ChromeMiniInstaller::LaunchAndCloseChrome(bool over_install) {
  VerifyChromeLaunch(true);
  if ((install_type_ == mini_installer_constants::kSystemInstall) &&
      (!over_install))
    CloseFirstRunUIDialog(over_install);
  CloseProcesses(installer_util::kChromeExe);
}

// This method will get Chrome exe path and launch it.
void ChromeMiniInstaller::VerifyChromeLaunch(bool expected_status) {
  std::wstring launch_path;
  GetChromeLaunchPath(&launch_path);
  base::LaunchApp(L"\"" + launch_path + L"\"", false, false, NULL);
  PlatformThread::Sleep(400);
  VerifyProcessLaunch(installer_util::kChromeExe, expected_status);
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
// Verifies if the process starts running.
void ChromeMiniInstaller::VerifyProcessLaunch(
    const wchar_t* process_name, bool expected_status) {
  int timer = 0, wait_time = 60000;
  if (!expected_status)
    wait_time = 8000;
  while ((base::GetProcessCount(process_name, NULL) == 0) &&
         (timer < wait_time)) {
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  if (expected_status)
    ASSERT_NE(0, base::GetProcessCount(process_name, NULL));
  else
    ASSERT_EQ(0, base::GetProcessCount(process_name, NULL));
}

// Verifies if the process stops running.
void ChromeMiniInstaller::VerifyProcessClose(
    const wchar_t* process_name) {
  int timer = 0;
  if (base::GetProcessCount(process_name, NULL) > 0) {
    printf("\nWaiting for this process to end... %ls\n", process_name);
    while ((base::GetProcessCount(process_name, NULL) > 0) &&
           (timer < 60000)) {
      PlatformThread::Sleep(200);
      timer = timer + 200;
    }
  } else {
    ASSERT_EQ(0, base::GetProcessCount(process_name, NULL));
  }
}
