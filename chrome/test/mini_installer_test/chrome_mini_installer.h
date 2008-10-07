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
  ChromeMiniInstaller() {}
  ~ChromeMiniInstaller() {}

  // Installs Chrome Mini Installer.
  void InstallMiniInstaller(bool over_install = false);

  // Installs ChromeSetupDev.exe.
  void InstallChromeSetupDev();

  // This method takes care of all overinstall cases.
  void OverInstall();

  // Uninstalls Chrome.
  void UnInstall();

  // Verifies if Chrome launches after overinstall.
  void VerifyChromeLaunch();

 private:
  // Closes Chrome uninstall confirm dialog window.
  bool CloseWindow(LPCWSTR window_name, UINT message);

  // Closes Chrome browser.
  void CloseChromeBrowser(LPCWSTR window_name);

  // Closes specified process.
  void CloseProcesses(const std::wstring& executable_name);

  // Checks for registry key.
  bool CheckRegistryKey(std::wstring key_path);

  // Deletes App folder after uninstall.
  void DeleteAppFolder();

  // This method verifies Chrome shortcut.
  void FindChromeShortcut();

  // Get path for uninstall.
  std::wstring GetUninstallPath();

  // Reads registry key.
  std::wstring GetRegistryKey();

  // Launches the given EXE and waits for it to end.
  void LaunchExe(std::wstring install_path, const wchar_t process_name[]);

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

