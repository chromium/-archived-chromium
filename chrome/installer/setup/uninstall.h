// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares Chrome uninstall related functions.

#ifndef CHROME_INSTALLER_SETUP_UNINSTALL_H__
#define CHROME_INSTALLER_SETUP_UNINSTALL_H__

#include <string>

#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

namespace installer_setup {
// This function uninstalls Chrome.
//
// exe_path: Path to the executable (setup.exe) as it will be copied
//           to temp folder before deleting Chrome folder.
// system_uninstall: if true, the function uninstalls Chrome installed system
//                   wise. otherwise, it uninstalls Chrome installed for the
//                   current user.
// installed_version: currently installed version of Chrome.
// remove_all: Remove all shared files, registry entries as well.
// force_uninstall: Uninstall without prompting for user confirmation or
//                  any checks for Chrome running.
installer_util::InstallStatus UninstallChrome(
    const std::wstring& exe_path, bool system_uninstall,
    const installer::Version& installed_version,
    bool remove_all, bool force_uninstall);

}  // namespace installer_setup

#endif  // CHROME_INSTALLER_SETUP_UNINSTALL_H__
