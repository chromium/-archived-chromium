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

#include "chrome/installer/util/util_constants.h"

namespace installer_util {

namespace switches {

// Disable logging
const wchar_t kDisableLogging[] = L"disable-logging";

// Enable logging at the error level. This is the default behavior.
const wchar_t kEnableLogging[] = L"enable-logging";

// Specify the file path of Chrome archive for install.
const wchar_t kInstallArchive[] = L"install-archive";

// If present, specify file path to write logging info.
const wchar_t kLogFile[] = L"log-file";

// Register Chrome as a valid browser on the current sytem. This option
// requires that setup.exe is running as admin. If this option is specified,
// options kInstallArchive and kUninstall are ignored.
const wchar_t kRegisterChromeBrowser[] = L"register-chrome-browser";

// By default we remove all shared (between users) files, registry entries etc
// during uninstall. If this option is specified together with kUninstall option
// we do not clean up shared entries otherwise this option is ignored.
const wchar_t kDoNotRemoveSharedItems[] = L"do-not-remove-shared-items";

// Install Chrome to system wise location. The default is per user install.
const wchar_t kSystemInstall[] = L"system-install";

// If present, setup will uninstall chrome.
const wchar_t kUninstall[] = L"uninstall";

// Enable verbose logging (info level).
const wchar_t kVerboseLogging[] = L"verbose-logging";

}  // namespace switches

const wchar_t kInstallBinaryDir[] = L"Application";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kChromeDll[] = L"chrome.dll";
const wchar_t kSetupExe[] = L"setup.exe";

const wchar_t kUninstallStringField[] = L"UninstallString";
const wchar_t kUninstallDisplayNameField[] = L"DisplayName";
}  // namespace installer_util
