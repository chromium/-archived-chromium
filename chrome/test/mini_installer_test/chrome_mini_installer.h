// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H__
#define CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H__

#include <windows.h>
#include <process.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <errno.h>
#include <string>
#include <vector>

#include "base/basictypes.h"

// This structure holds the name and creation time
// details of all the chrome nightly builds.
class file_info {
 public:
  file_info() {}
  file_info(const std::wstring& in_name, int in_creation_time) {
    name_.assign(in_name);
    creation_time_ = in_creation_time;
  }
  // This is a predicate to sort file information.
  bool IsNewer(const file_info& creation_time_begin,
               const file_info& creation_time_end);

  std::wstring name_;
  int creation_time_;
};
typedef std::vector<file_info> file_info_list;

// This class has methods to install and uninstall Chrome mini installer.
class ChromeMiniInstaller {
 public:
  explicit ChromeMiniInstaller(std::wstring install_type,
                               std::wstring build_type) {
    install_type_ = install_type;
    build_channel_ = build_type;
  }

  ~ChromeMiniInstaller() {}

  enum RepairChrome {
    REGISTRY,
    VERSION_FOLDER
  };

  // Closes specified process.
  void CloseProcesses(const std::wstring& executable_name);

  // This method returns path to either program files
  // or documents and setting based on the install type.
  std::wstring GetChromeInstallDirectoryLocation();

  // Installs the latest full installer.
  void InstallFullInstaller(bool over_install);

  // Installs chrome.
  void Install();

  // This method will first install the full installer and
  // then over installs with diff installer.
  void OverInstallOnFullInstaller(const std::wstring& install_type);

  // Installs Google Chrome through meta installer.
  void InstallMetaInstaller();

  // Installs Chrome Mini Installer.
  void InstallMiniInstaller(bool over_install = false,
                            const std::wstring& path = L"");

  // This will test the standalone installer.
  void InstallStandaloneInstaller();

  // Repairs Chrome based on the passed argument.
  void Repair(RepairChrome repair_type);

  // Uninstalls Chrome.
  void UnInstall();

  // This method will perform a over install
  void OverInstall();

 private:
  // This variable holds the install type.
  // Install type can be either system or user level.
  std::wstring install_type_;

  // This variable holds the channel information. A dev build or stable build
  // will be installed based on the build_channel_ value.
  std::wstring build_channel_;

  bool standalone_installer;

  // Will clean up the machine if Chrome install is messed up.
  void CleanChromeInstall();

  // Closes First Run UI dialog.
  void CloseFirstRunUIDialog(bool over_install);

  // Close Window whose name is 'window_name', by sending Windows message
  // 'message' to it.
  bool CloseWindow(const wchar_t* window_name, UINT message);

  // Closes Chrome uninstall confirm dialog window.
  bool CloseUninstallWindow();

  // Closes Chrome browser.
  bool CloseChromeBrowser();

  // Change current directory so that chrome.dll from current folder
  // will not be used as fall back.
  void ChangeCurrentDirectory(std::wstring *current_path);

  // Checks for registry key.
  bool CheckRegistryKey(const std::wstring& key_path);

  // Checks for registry key on uninstall.
  bool CheckRegistryKeyOnUninstall(const std::wstring& key_path);

  // Deletes specified folder after getting the install path.
  void DeleteFolder(const wchar_t* folder_name);

  // Will delete user data profile.
  void DeleteUserDataFolder();

  // This will delete pv key from client registry.
  void DeletePvRegistryKey();

  // This method verifies Chrome shortcut.
  void FindChromeShortcut();

  // This method will create a command line to run apply tag.
  bool GetCommandForTagging(std::wstring *return_command);

  // This method will get the latest installer based
  // on the pattern argument passed.
  bool GetInstaller(const wchar_t* pattern, std::wstring *name);

  // Gets path for the specified parameter.
  std::wstring GetFilePath(const wchar_t* name);

  // This method will get the latest installer filename,
  // based on the passed pattern argument.
  bool GetLatestFile(const wchar_t* path,
                     const wchar_t* pattern,
                     file_info_list *file_name);

  // This method will get the previous full installer based on
  // the diff installer file name argument.
  bool GetPreviousFullInstaller(const std::wstring& diff_file,
                                std::wstring *previous);

  // This method will get the previous build number
  void GetPreviousBuildNumber(const std::wstring& path,
                              std::wstring *build_number);

  // Get HKEY based on install type.
  HKEY GetRootRegistryKey();

  // Returns Chrome pv registry key value
  bool GetChromeVersionFromRegistry(std::wstring *return_reg_key_value);

  // This method gets the shortcut path from start menu based on install type.
  std::wstring GetStartMenuShortcutPath();

  // This method will return standalone installer file name.
  bool GetStandaloneInstallerFileName(file_info_list *file_name);

  // This method will get the version number from the filename.
  bool GetStandaloneVersion(std::wstring* version);

  // Get path for uninstall.
  std::wstring GetUninstallPath();

  // Gets the path to launch Chrome.
  bool GetChromeLaunchPath(std::wstring* launch_path);

  // This method will get Chrome.exe path and launch it.
  void VerifyChromeLaunch(bool expected_status);

  // Launches the chrome installer and waits for it to end.
  void LaunchInstaller(const std::wstring& install_path,
                       const wchar_t* process_name);

  // This method will send enter key to window in the foreground.
  void SendEnterKeyToWindow();

  // Verifies if Chrome launches after install.
  void LaunchAndCloseChrome(bool over_install);

  // Compares the registry key values after overinstall.
  bool VerifyOverInstall(const std::wstring& reg_key_value_before_overinstall,
                         const std::wstring& reg_key_value_after_overinstall);

  // Checks if the differential install is correct.
  bool VerifyDifferentialInstall(const std::wstring& full_installer_value,
                                 const std::wstring& diff_installer_value,
                                 const std::wstring& diff_installer_name);

  // This method will verify if the installed build is correct.
  bool VerifyStandaloneInstall();

  // Verifies if the given process starts running.
  void VerifyProcessLaunch(const wchar_t* process_name, bool expected_status);

  // Verifies if the given process stops running.
  void VerifyProcessClose(const wchar_t* process_name);

  DISALLOW_COPY_AND_ASSIGN(ChromeMiniInstaller);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H__
