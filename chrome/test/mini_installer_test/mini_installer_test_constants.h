// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants related to the Chrome mini installer testing.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_CONSTANTS_H__
#define CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_CONSTANTS_H__

namespace mini_installer_constants {

// Path and process names
extern const wchar_t kChromeAppDir[];
extern const wchar_t kChromeSetupExecutable[];
extern const wchar_t kChromeMiniInstallerExecutable[];
extern const wchar_t kChromeMetaInstallerExecutable[];
extern const wchar_t kGoogleUpdateExecutable[];
extern const wchar_t kIEExecutable[];

// Window names.
extern const wchar_t kBrowserAppName[];
extern const wchar_t kBrowserTabName[];
extern const wchar_t kChromeBuildType[];
extern const wchar_t kChromeFirstRunUI[];
extern const wchar_t kInstallerWindow[];

// Shortcut names
extern const wchar_t kChromeLaunchShortcut[];
extern const wchar_t kChromeUninstallShortcut[];

// Chrome install types
extern const wchar_t kSystemInstall[];
extern const wchar_t kStandaloneInstaller[];
extern const wchar_t kUserInstall[];

// Google Chrome meta installer location.
extern const wchar_t kChromeApplyTagExe[];
extern const wchar_t kChromeMetaInstallerExe[];
extern const wchar_t kChromeStandAloneInstallerLocation[];
extern const wchar_t kChromeApplyTagParameters[];
}

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_CONSTANTS_H__

