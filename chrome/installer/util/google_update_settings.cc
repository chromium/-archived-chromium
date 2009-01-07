// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/registry.h"
#include "chrome/installer/util/google_update_constants.h"

namespace {

std::wstring GetClientStateKeyPath() {
  std::wstring reg_path(google_update::kRegPathClientState);
  reg_path.append(L"\\");
  reg_path.append(google_update::kChromeGuid);
  return reg_path;
}

bool ReadGoogleUpdateStrKey(const wchar_t* const name, std::wstring* value) {
  std::wstring reg_path = GetClientStateKeyPath();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  if (!key.ReadValue(name, value)) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    return hklm_key.ReadValue(name, value);
  }
  return true;
}

bool ClearGoogleUpdateStrKey(const wchar_t* const name) {
  std::wstring reg_path = GetClientStateKeyPath();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  std::wstring value;
  if (!key.ReadValue(name, &value))
    return false;
  return key.WriteValue(name, L"");
}

}  // namespace.

bool GoogleUpdateSettings::GetCollectStatsConsent() {
  std::wstring reg_path = GetClientStateKeyPath();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  DWORD value;
  if (!key.ReadValueDW(google_update::kRegUsageStatsField, &value)) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    if (!hklm_key.ReadValueDW(google_update::kRegUsageStatsField, &value))
      return false;
  }
  return (1 == value);
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  std::wstring reg_path = GetClientStateKeyPath();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  DWORD value = consented ? 1 : 0;
  return key.WriteValue(google_update::kRegUsageStatsField, value);
}

bool GoogleUpdateSettings::GetBrowser(std::wstring* browser) {
  return ReadGoogleUpdateStrKey(google_update::kRegBrowserField, browser);
}

bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  return ReadGoogleUpdateStrKey(google_update::kRegLangField, language);
}

bool GoogleUpdateSettings::GetBrand(std::wstring* brand) {
  return ReadGoogleUpdateStrKey(google_update::kRegRLZBrandField, brand);
}

bool GoogleUpdateSettings::GetReferral(std::wstring* referral) {
  return ReadGoogleUpdateStrKey(google_update::kRegReferralField, referral);
}

bool GoogleUpdateSettings::ClearReferral() {
  return ClearGoogleUpdateStrKey(google_update::kRegReferralField);
}
