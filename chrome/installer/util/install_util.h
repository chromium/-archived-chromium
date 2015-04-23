// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares utility functions for the installer. The original reason
// for putting these functions in installer\util library is so that we can
// separate out the critical logic and write unit tests for it.

#ifndef CHROME_INSTALLER_UTIL_INSTALL_UTIL_H__
#define CHROME_INSTALLER_UTIL_INSTALL_UTIL_H__

#include <tchar.h>
#include <windows.h>
#include <string>

#include "base/basictypes.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

class WorkItemList;

// This is a utility class that provides common installation related
// utility methods that can be used by installer and also unit tested
// independently.
class InstallUtil {
 public:
  // Launches given exe as admin on Vista.
  static bool ExecuteExeAsAdmin(const std::wstring& exe,
                                const std::wstring& params,
                                DWORD* exit_code);

  // Reads the uninstall command for Chromium from registry and returns it.
  // If system_install is true the command is read from HKLM, otherwise
  // from HKCU.
  static std::wstring GetChromeUninstallCmd(bool system_install);
  // Find the version of Chrome installed on the system by checking the
  // Google Update registry key. Returns the version or NULL if no version is
  // found.
  // system_install: if true, looks for version number under the HKLM root,
  //                 otherwise looks under the HKCU.
  static installer::Version* GetChromeVersion(bool system_install);

  // This function checks if the current OS is supported for Chromium.
  static bool IsOSSupported();

  // This function sets installer error information in registry so that Google
  // Update can read it and display to the user.
  static void WriteInstallerResult(bool system_install,
                                   installer_util::InstallStatus status,
                                   int string_resource_id,
                                   const std::wstring* const launch_cmd);

  // Returns true if this installation path is per user, otherwise returns
  // false (per machine install, meaning: the exe_path contains path to
  // Program Files).
  static bool IsPerUserInstall(const wchar_t* const exe_path);

  // Adds all DLLs in install_path whose names are given by dll_names to a
  // work item list containing registration or unregistration actions.
  //
  // install_path: Install path containing the registrable DLLs.
  // dll_names: the array of strings containing dll_names
  // dll_names_count: the number of DLL names in dll_names
  // do_register: whether to register or unregister the DLLs
  // registration_list: the WorkItemList that this method populates
  //
  // Returns true if at least one DLL was successfully added to
  // registration_list.
  static bool BuildDLLRegistrationList(const std::wstring& install_path,
                                       const wchar_t** const dll_names,
                                       int dll_names_count,
                                       bool do_register,
                                       WorkItemList* registration_list);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(InstallUtil);
};


#endif  // CHROME_INSTALLER_UTIL_INSTALL_UTIL_H__
