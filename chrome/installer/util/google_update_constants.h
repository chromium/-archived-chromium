// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used with Google Update.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H_

namespace google_update {

extern const wchar_t kChromeGuid[];
// Strictly speaking Google Update doesn't care about this GUID but it is still
// related to install as it is used by MSI to identify Gears.
extern const wchar_t kGearsUpgradeCode[];

extern const wchar_t kRegPathClients[];

// The difference between ClientState and ClientStateMedium is that the former
// lives on HKCU or HKLM and the later always lives in HKLM. The only use of
// the ClientStateMedium is for the EULA consent. See bug 1594565.
extern const wchar_t kRegPathClientState[];
extern const wchar_t kRegPathClientStateMedium[];

extern const wchar_t kRegApField[];
extern const wchar_t kRegBrowserField[];
extern const wchar_t kRegDidRunField[];
extern const wchar_t kRegLangField[];
extern const wchar_t kRegLastCheckedField[];
extern const wchar_t kRegNameField[];
extern const wchar_t kRegOldVersionField[];
extern const wchar_t kRegRenameCmdField[];
extern const wchar_t kRegRLZBrandField[];
extern const wchar_t kRegUsageStatsField[];
extern const wchar_t kRegVersionField[];
extern const wchar_t kRegReferralField[];
extern const wchar_t kRegEULAAceptedField[];

extern const wchar_t kEnvProductVersionKey[];
}  // namespace google_update

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H_
