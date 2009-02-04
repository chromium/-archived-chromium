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

#include "base/file_util.h"

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
  void InstallMiniInstaller(bool over_install = false);

  // Uninstalls Chrome.
  void UnInstall();

  // This method takes care of all overinstall cases.
  void OverInstall();

 private:
  // This variable holds the install type.
  // Install type can be either system or user level.
  std::wstring install_type_;

  // Closes First Run UI dialog.
  void CloseFirstRunUIDialog(bool over_install);

  // Closes Chrome uninstall confirm dialog window.
  bool CloseWindow(LPCWSTR window_name, UINT message);

  // Closes Chrome browser.
  void CloseChromeBrowser(LPCWSTR window_name);

  // Checks for registry key.
  bool CheckRegistryKey(std::wstring key_path);

  // Deletes App folder after uninstall.
  void DeleteAppFolder();

  // This method verifies Chrome shortcut.
  void FindChromeShortcut();

  // This method returns path to either program files
  // or documents and setting based on the install type.
  std::wstring GetChromeInstallDirectoryLocation();

  // Get HKEY based on install type.
  HKEY GetRootRegistryKey();

  // Get path for mini_installer.exe.
  std::wstring GetMiniInstallerExePath();

  // This method gets the shortcut path from start menu based on install type.
  std::wstring GetStartMenuShortcutPath();

  // Get path for uninstall.
  std::wstring GetUninstallPath();

  // Returns Chrome pv registry key value
  bool GetRegistryKey(std::wstring *return_reg_key_value);

  // Checks for the build type
  bool IsChromiumBuild();

  // Launches the chrome installer and waits for it to end.
  void LaunchInstaller(std::wstring install_path, const wchar_t process_name[]);

  // Verifies if Chrome launches after install.
  void VerifyChromeLaunch();

  // Compares the registry key values after overinstall.
  bool VerifyOverInstall(std::wstring reg_key_value_before_overinstall,
                         std::wstring reg_key_value_after_overinstall);

  // Waits until the given process starts running
  void WaitUntilProcessStartsRunning(const wchar_t process_name[]);

  // Waits until the given process stops running
  void WaitUntilProcessStopsRunning(const wchar_t process_name[]);

  DISALLOW_EVIL_CONSTRUCTORS(ChromeMiniInstaller);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_CHROME_MINI_INSTALLER_H__

