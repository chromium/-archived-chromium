// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines all install related constants that need to be used by Chrome as
// well as Chrome Installer.

#ifndef CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H__
#define CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H__

namespace installer_util {

// Return status of installer
enum InstallStatus {
  FIRST_INSTALL_SUCCESS, // Successfully installed Chrome for the first time
  INSTALL_REPAIRED,      // Same version reinstalled for repair
  NEW_VERSION_UPDATED,   // Chrome successfully updated to new version
  HIGHER_VERSION_EXISTS, // Higher version of Chrome already exists
  USER_LEVEL_INSTALL_EXISTS, // User level install already exists 
  SYSTEM_LEVEL_INSTALL_EXISTS, // Machine level install already exists 
  INSTALL_FAILED,        // Install/update failed
  OS_NOT_SUPPORTED,      // Current OS not supported
  OS_ERROR,              // OS API call failed
  TEMP_DIR_FAILED,       // Unable to get Temp directory
  UNCOMPRESSION_FAILED,  // Failed to uncompress Chrome archive
  INVALID_ARCHIVE,       // Something wrong with the installer archive
  CHROME_NOT_INSTALLED,  // Chrome not installed (returned in case of uninstall)
  CHROME_RUNNING,        // Chrome currently running (when trying to uninstall)
  UNINSTALL_CONFIRMED,   // User has confirmed Chrome uninstall
  UNINSTALL_SUCCESSFUL,  // Chrome successfully uninstalled
  UNINSTALL_FAILED,      // Chrome uninstallation failed
  UNINSTALL_CANCELLED,   // User cancelled Chrome uninstallation
  UNKNOWN_STATUS,        // Unknown status (this should never happen)
};

namespace switches {
extern const wchar_t kCreateAllShortcuts[];
extern const wchar_t kDisableLogging[];
extern const wchar_t kDoNotLaunchChrome[];
extern const wchar_t kDoNotRemoveSharedItems[];
extern const wchar_t kEnableLogging[];
extern const wchar_t kForceUninstall[];
extern const wchar_t kInstallArchive[];
extern const wchar_t kLogFile[];
extern const wchar_t kMakeChromeDefault[];
extern const wchar_t kRegisterChromeBrowser[];
extern const wchar_t kSystemLevel[];
extern const wchar_t kUninstall[];
extern const wchar_t kVerboseLogging[];
}  // namespace switches

extern const wchar_t kInstallBinaryDir[];
extern const wchar_t kChromeExe[];
extern const wchar_t kChromeDll[];
extern const wchar_t kSetupExe[];

extern const wchar_t kUninstallStringField[];
extern const wchar_t kUninstallDisplayNameField[];
}  // namespace installer_util

#endif  // CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H__

