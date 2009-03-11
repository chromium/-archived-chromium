// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_constants.h"

namespace google_update {

const wchar_t kChromeGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kGearsUpgradeCode[] = L"{D92DBAED-3E3E-4530-B30D-072D16C7DDD0}";

const wchar_t kRegPathClients[] = L"Software\\Google\\Update\\Clients";
const wchar_t kRegPathClientState[] = L"Software\\Google\\Update\\ClientState";
const wchar_t kRegPathClientStateMedium[]
    = L"Software\\Google\\Update\\ClientStateMedium";

const wchar_t kRegApField[] = L"ap";
const wchar_t kRegBrowserField[] = L"browser";
const wchar_t kRegDidRunField[] = L"dr";
const wchar_t kRegLangField[] = L"lang";
const wchar_t kRegLastCheckedField[] = L"LastChecked";
const wchar_t kRegNameField[] = L"name";
const wchar_t kRegOldVersionField[] = L"opv";
const wchar_t kRegRenameCmdField[] = L"cmd";
const wchar_t kRegRLZBrandField[] = L"brand";
const wchar_t kRegUsageStatsField[] = L"usagestats";
const wchar_t kRegVersionField[] = L"pv";
const wchar_t kRegReferralField[] = L"referral";
const wchar_t kRegEULAAceptedField[] = L"eulaaccepted";

const wchar_t kEnvProductVersionKey[] = L"CHROME_VERSION";
}  // namespace installer
