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
installer_util::InstallStatus UninstallChrome(
    const std::wstring& exe_path, bool system_uninstall,
    const installer::Version& installed_version, bool remove_all);

}  // namespace installer_setup

#endif  // CHROME_INSTALLER_SETUP_UNINSTALL_H__
