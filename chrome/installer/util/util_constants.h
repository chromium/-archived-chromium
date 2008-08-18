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
extern const wchar_t kDisableLogging[];
extern const wchar_t kEnableLogging[];
extern const wchar_t kInstallArchive[];
extern const wchar_t kLogFile[];
extern const wchar_t kRegisterChromeBrowser[];
extern const wchar_t kDoNotRemoveSharedItems[];
extern const wchar_t kSystemInstall[];
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
