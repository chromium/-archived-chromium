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

  // Closes all running Chrome processes.
  void CloseChromeProcesses();

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
