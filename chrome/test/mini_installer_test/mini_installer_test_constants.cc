// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"

namespace mini_installer_constants {

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kChromeAppDir[] = L"Google\\Chrome\\Application\\";
const wchar_t kChromeBuildType[] = L"Google Chrome";
const wchar_t kChromeFirstRunUI[] = L"Welcome to Google Chrome";
const wchar_t kChromeLaunchShortcut[] = L"Google Chrome.lnk";
const wchar_t kChromeUninstallShortcut[] = L"Uninstall Google Chrome.lnk";
const wchar_t kChromeUninstallDialogName[] = L"Uninstall Google Chrome";
#else
const wchar_t kChromeAppDir[] = L"Chromium\\Application\\";
const wchar_t kChromeBuildType[] = L"Chromium";
const wchar_t kChromeFirstRunUI[] = L"Welcome to Chromium";
const wchar_t kChromeLaunchShortcut[] = L"Chromium.lnk";
const wchar_t kChromeUninstallShortcut[] = L"Uninstall Chromium.lnk";
const wchar_t kChromeUninstallDialogName[] = L"Uninstall Chromium";
#endif
const wchar_t kBrowserAppName[] = L"Google - Google Chrome";
const wchar_t kBrowserTabName[] = L"New Tab - Google Chrome";
const wchar_t kChromeMiniInstallerExecutable[] = L"mini_installer.exe";
const wchar_t kChromeMetaInstallerExecutable[] = L"chrome_installer.exe";
const wchar_t kChromeSetupExecutable[] = L"setup.exe";
const wchar_t kChromeUserDataDir[] = L"User Data";
const wchar_t kChromeUserDataBackupDir[] = L"User Data Copy";
const wchar_t kDiffInstall[] = L"Diff";
const wchar_t kDiffInstallerPattern[] = L"_from_";
const wchar_t kFullInstallerPattern[] = L"_chrome_installer";
const wchar_t kGoogleUpdateExecutable[] = L"GoogleUpdate.exe";
const wchar_t kIEExecutable[] = L"iexplore.exe";
const wchar_t kInstallerWindow[] = L"Chrome Installer";
const wchar_t kSystemInstall[] = L"system";
const wchar_t kStandaloneInstaller[] = L"ChromeSetupTest.exe";
const wchar_t kUserInstall[] = L"user";
const wchar_t kUntaggedInstallerPattern[] = L"ChromeStandaloneSetup_";


const wchar_t kDevChannelBuild[] = L"3.0.";
const wchar_t kStableChannelBuild[] = L"2.0.";
const wchar_t kFullInstall[] = L"Full";



// Google Chrome meta installer location.
const wchar_t kChromeMetaInstallerExe[] =
    L"\\\\172.23.44.61\\shared\\chrome_autotest\\beta_build\\ChromeSetup.exe";
const wchar_t kChromeStandAloneInstallerLocation[] =
    L"\\\\172.24.6.7\\shares\\googleclient\\nightly\\builds\\Win-OmahaInstallers\\latest\\opt-win\\staging\\";
const wchar_t kChromeApplyTagExe[] =
    L"\\\\172.23.44.61\\shared\\chrome_autotest\\ApplyTag.exe";
const wchar_t kChromeApplyTagParameters[] =
    L"\"appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Chrome&needsadmin=false\"";
const wchar_t kChromeDiffInstallerLocation[] =
    L"\\\\172.24.6.7\\shares\\googleclient\\save\\builds\\chrome\\";
}
