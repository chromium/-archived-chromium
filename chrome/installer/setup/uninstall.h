// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares Chrome uninstall related functions.

#ifndef CHROME_INSTALLER_SETUP_UNINSTALL_H__
#define CHROME_INSTALLER_SETUP_UNINSTALL_H__

#include <string>

#include <shlobj.h>

#include "base/command_line.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

namespace installer_setup {
// This function removes all Chrome registration related keys. It returns true
// if successful, otherwise false. The error code is set in |exit_code|.
// |root| is the registry root (HKLM|HKCU)
bool DeleteChromeRegistrationKeys(HKEY root,
                                  installer_util::InstallStatus& exit_code);

// This function uninstalls Chrome.
//
// exe_path: Path to the executable (setup.exe) as it will be copied
//           to temp folder before deleting Chrome folder.
// system_uninstall: if true, the function uninstalls Chrome installed system
//                   wise. otherwise, it uninstalls Chrome installed for the
//                   current user.
// remove_all: Remove all shared files, registry entries as well.
// force_uninstall: Uninstall without prompting for user confirmation or
//                  any checks for Chrome running.
// cmd_line: CommandLine that contains information about the command that
//           was used to launch current uninstaller.
// cmd_params: Command line parameters passed to the uninstaller.
installer_util::InstallStatus UninstallChrome(
    const std::wstring& exe_path, bool system_uninstall,
    bool remove_all, bool force_uninstall,
    const CommandLine& cmd_line, const wchar_t* cmd_params);

}  // namespace installer_setup

#endif  // CHROME_INSTALLER_SETUP_UNINSTALL_H__
