// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/util_constants.h"

namespace installer_util {

namespace switches {

// Create Desktop and QuickLaunch shortcuts
const wchar_t kCreateAllShortcuts[] = L"create-all-shortcuts";

// Disable logging
const wchar_t kDisableLogging[] = L"disable-logging";

// Prevent installer from launching Chrome after a successful first install.
const wchar_t kDoNotLaunchChrome[] = L"do-not-launch-chrome";

// By default we remove all shared (between users) files, registry entries etc
// during uninstall. If this option is specified together with kUninstall option
// we do not clean up shared entries otherwise this option is ignored.
const wchar_t kDoNotRemoveSharedItems[] = L"do-not-remove-shared-items";

// Enable logging at the error level. This is the default behavior.
const wchar_t kEnableLogging[] = L"enable-logging";

// If present, setup will uninstall chrome without asking for any
// confirmation from user.
const wchar_t kForceUninstall[] = L"force-uninstall";

// Specify the file path of Chrome archive for install.
const wchar_t kInstallArchive[] = L"install-archive";

// Specify the file path of Chrome master preference file.
const wchar_t kInstallerData[] = L"installerdata";

// If present, specify file path to write logging info.
const wchar_t kLogFile[] = L"log-file";

// Register Chrome as default browser on the system. Usually this will require
// that setup is running as admin. If running as admin we try to register
// as default browser at system level, if running as non-admin we try to
// register as default browser only for the current user. 
const wchar_t kMakeChromeDefault[] = L"make-chrome-default";

// Register Chrome as a valid browser on the current sytem. This option
// requires that setup.exe is running as admin. If this option is specified,
// options kInstallArchive and kUninstall are ignored.
const wchar_t kRegisterChromeBrowser[] = L"register-chrome-browser";

// Renames chrome.exe to old_chrome.exe and renames new_chrome.exe to chrome.exe
// to support in-use updates. Also deletes opv key.
const wchar_t kRenameChromeExe[] = L"rename-chrome-exe";

// When we try to relaunch setup.exe as admin on Vista, we append this command
// line flag so that we try the launch only once.
const wchar_t kRunAsAdmin[] = L"run-as-admin";

// Install Chrome to system wise location. The default is per user install.
const wchar_t kSystemLevel[] = L"system-level";

// If present, setup will uninstall chrome.
const wchar_t kUninstall[] = L"uninstall";

// Enable verbose logging (info level).
const wchar_t kVerboseLogging[] = L"verbose-logging";

// Show the embedded EULA dialog.
const wchar_t kShowEula[] = L"show-eula";

}  // namespace switches

const wchar_t kInstallBinaryDir[] = L"Application";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kChromeOldExe[] = L"old_chrome.exe";
const wchar_t kChromeNewExe[] = L"new_chrome.exe";
const wchar_t kChromeDll[] = L"chrome.dll";
const wchar_t kSetupExe[] = L"setup.exe";

const wchar_t kUninstallStringField[] = L"UninstallString";
const wchar_t kUninstallDisplayNameField[] = L"DisplayName";
}  // namespace installer_util
