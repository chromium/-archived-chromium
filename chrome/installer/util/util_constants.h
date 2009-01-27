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
  INSUFFICIENT_RIGHTS,   // User trying system level install is not Admin
  CHROME_NOT_INSTALLED,  // Chrome not installed (returned in case of uninstall)
  CHROME_RUNNING,        // Chrome currently running (when trying to uninstall)
  UNINSTALL_CONFIRMED,   // User has confirmed Chrome uninstall
  UNINSTALL_SUCCESSFUL,  // Chrome successfully uninstalled
  UNINSTALL_FAILED,      // Chrome uninstallation failed
  UNINSTALL_CANCELLED,   // User cancelled Chrome uninstallation
  UNKNOWN_STATUS,        // Unknown status (this should never happen)
  RENAME_SUCCESSFUL,     // Rename of new_chrome.exe to chrome.exe worked
  RENAME_FAILED,         // Rename of new_chrome.exe failed
  EULA_REJECTED,         // EULA dialog was not accepted by user.
  EULA_ACCEPTED          // EULA dialog was accepted by user.
};

// These are distibution related install options specified through command
// line switches (see below) or master preference file (see
// chrome/installer/util/master_preference.h). The options can be combined,
// so they are bit flags.
enum InstallOption {
  // A master profile file is provided to installer.
  MASTER_PROFILE_PRESENT  = 0x1,
  // The master profile file provided is valid.
  MASTER_PROFILE_VALID    = 0x1 << 1,
  // Create Desktop and QuickLaunch shortcuts.
  CREATE_ALL_SHORTCUTS    = 0x1 << 2,
  // Prevent installer from launching Chrome after a successful first install.
  DO_NOT_LAUNCH_CHROME    = 0x1 << 3,
  // Register Chrome as default browser on the system.
  MAKE_CHROME_DEFAULT     = 0x1 << 4,
  // Install Chrome to system wise location.
  SYSTEM_LEVEL            = 0x1 << 5,
  // Run installer in verbose mode.
  VERBOSE_LOGGING         = 0x1 << 6,
  // Show the EULA dialog.
  SHOW_EULA_DIALOG        = 0x1 << 7
};

namespace switches {
extern const wchar_t kCreateAllShortcuts[];
extern const wchar_t kDisableLogging[];
extern const wchar_t kDoNotLaunchChrome[];
extern const wchar_t kDoNotRemoveSharedItems[];
extern const wchar_t kEnableLogging[];
extern const wchar_t kForceUninstall[];
extern const wchar_t kInstallArchive[];
extern const wchar_t kInstallerData[];
extern const wchar_t kLogFile[];
extern const wchar_t kMakeChromeDefault[];
extern const wchar_t kRegisterChromeBrowser[];
extern const wchar_t kRenameChromeExe[];
extern const wchar_t kRunAsAdmin[];
extern const wchar_t kSystemLevel[];
extern const wchar_t kUninstall[];
extern const wchar_t kVerboseLogging[];
extern const wchar_t kShowEula[];
}  // namespace switches

extern const wchar_t kInstallBinaryDir[];
extern const wchar_t kChromeExe[];
extern const wchar_t kChromeOldExe[];
extern const wchar_t kChromeNewExe[];
extern const wchar_t kChromeDll[];
extern const wchar_t kSetupExe[];

extern const wchar_t kUninstallStringField[];
extern const wchar_t kUninstallDisplayNameField[];
}  // namespace installer_util

#endif  // CHROME_INSTALLER_UTIL_UTIL_CONSTANTS_H__
