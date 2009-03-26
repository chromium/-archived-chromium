// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
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

#include "base/file_util.h"

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
  ChromeMiniInstaller(std::wstring type) {
    install_type_ = type;
  }

  ~ChromeMiniInstaller() {}

  // Closes specified process.
  void CloseProcesses(const std::wstring& executable_name);

  // Installs Google Chrome through meta installer.
  void InstallMetaInstaller();

  // Installs Chrome Mini Installer.
  void InstallMiniInstaller(bool over_install = false,
                            const wchar_t* exe_name = L"");

  // This will test the standalone installer.
  void InstallStandaloneIntaller();

  // Uninstalls Chrome.
  void UnInstall();

  // This method takes care of all overinstall cases.
  void OverInstall();

 private:
  // This variable holds the install type.
  // Install type can be either system or user level.
  std::wstring install_type_;

  bool standalone_installer;

  // Closes First Run UI dialog.
  void CloseFirstRunUIDialog(bool over_install);

  // Closes Chrome uninstall confirm dialog window.
  bool CloseWindow(const wchar_t* window_name, UINT message);

  // Closes Chrome browser.
  void CloseChromeBrowser(const wchar_t* window_name);

  // Checks for registry key.
  bool CheckRegistryKey(const std::wstring& key_path);

  // Deletes App folder after uninstall.
  void DeleteAppFolder();

  // This method verifies Chrome shortcut.
  void FindChromeShortcut();

  // This method returns path to either program files
  // or documents and setting based on the install type.
  std::wstring GetChromeInstallDirectoryLocation();

  // This method will create a command line to run apply tag.
  bool GetCommandForTagging(std::wstring *return_command);

  // This method will get the latest installer based
  // on the pattern argument passed.
  bool GetInstaller(const wchar_t* pattern, std::wstring *name);

  // Get path for mini_installer.exe.
  std::wstring GetInstallerExePath(const wchar_t* installer_name);

  // This method will get the latest installer filename,
  // based on the passed pattern argument.
  bool GetLatestFile(const wchar_t* path,
                     const wchar_t* pattern,
                     file_info_list *file_name);

  // This method will get the previous full installer based on
  // the diff installer file name argument.
  bool GetPreviousFullInstaller(const std::wstring& diff_file,
                                std::wstring *previous);

  // Get HKEY based on install type.
  HKEY GetRootRegistryKey();

  // Returns Chrome pv registry key value
  bool GetRegistryKey(std::wstring *return_reg_key_value);

  // This method gets the shortcut path from start menu based on install type.
  std::wstring GetStartMenuShortcutPath();

  // This method will return standalone installer file name.
  bool GetStandaloneInstallerFileName(file_info_list *file_name);

  // This method will get the version number from the filename.
  bool GetStandaloneVersion(std::wstring* version);

  // Get path for uninstall.
  std::wstring GetUninstallPath();

  // Checks for the build type
  bool IsChromiumBuild();

  // Launches the chrome installer and waits for it to end.
  void LaunchInstaller(const std::wstring& install_path,
                       const wchar_t* process_name);

  // Verifies if Chrome launches after install.
  void VerifyChromeLaunch();

  // Compares the registry key values after overinstall.
  bool VerifyOverInstall(const std::wstring& reg_key_value_before_overinstall,
                         const std::wstring& reg_key_value_after_overinstall);

  // This method will verify if the installed build is correct.
  bool VerifyStandaloneInstall();

  // Waits until the given process starts running
  void WaitUntilProcessStartsRunning(const wchar_t* process_name);

  // Waits until the given process stops running
  void WaitUntilProcessStopsRunning(const wchar_t* process_name);

  DISALLOW_EVIL_CONSTRUCTORS(ChromeMiniInstaller);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H__
