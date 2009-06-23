// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the specification of setup main functions.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_H_
#define CHROME_INSTALLER_SETUP_INSTALL_H_

#include <string>
#include <windows.h>

#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

namespace installer {
// Get path to the installer under Chrome version folder
// (for example <path>\Google\Chrome\<Version>\installer)
std::wstring GetInstallerPathUnderChrome(const std::wstring& install_path,
                                         const std::wstring& new_version);

// This function installs or updates a new version of Chrome. It returns
// install status (failed, new_install, updated etc).
//
// exe_path: Path to the executable (setup.exe) as it will be copied
//           to Chrome install folder after install is complete
// archive_path: Path to the archive (chrome.7z) as it will be copied
//               to Chrome install folder after install is complete
// install_temp_path: working directory used during install/update. It should
//                    also has a sub dir source that contains a complete
//                    and unpacked Chrome package.
// options: install options. See chrome/installer/util/util_constants.h.
// new_version: new Chrome version that needs to be installed
// installed_version: currently installed version of Chrome, if any, or
//                    NULL otherwise
//
// Note: since caller unpacks Chrome to install_temp_path\source, the caller
// is responsible for cleaning up install_temp_path.
installer_util::InstallStatus InstallOrUpdateChrome(
    const std::wstring& exe_path, const std::wstring& archive_path,
    const std::wstring& install_temp_path, int options,
    const Version& new_version, const Version* installed_version);

// This function installs a new version of Chrome to the specified location.
// It returns true if install was successful and false in case of an error.
//
// exe_path: Path to the executable (setup.exe) as it will be copied
//           to Chrome install folder after install is complete
// archive_path: Path to the archive (chrome.7z) as it will be copied
//               to Chrome install folder after install is complete
// src_path: the path that contains a complete and unpacked Chrome package
//           to be installed.
// install_path: the destination path for Chrome to be installed to. This
//               path does not need to exist.
// temp_dir: the path of working directory used during installation. This path
//           does not need to exist.
// reg_root: the root of registry where the function applies settings for the
//           new Chrome version. It should be either HKLM or HKCU.
// new_version: new Chrome version that needs to be installed
//
// This function makes best effort to do installation in a transactional
// manner. If failed it tries to rollback all changes on the file system
// and registry. For example, if install_path exists before calling the
// function, it rolls back all new file and directory changes under
// install_path. If install_path does not exist before calling the function
// (typical new install), the function creates install_path during install
// and removes the whole directory during rollback.
bool InstallNewVersion(const std::wstring& exe_path,
                       const std::wstring& archive_path,
                       const std::wstring& src_path,
                       const std::wstring& install_path,
                       const std::wstring& temp_dir,
                       const HKEY reg_root,
                       const Version& new_version);

}

#endif  // CHROME_INSTALLER_SETUP_INSTALL_H_
