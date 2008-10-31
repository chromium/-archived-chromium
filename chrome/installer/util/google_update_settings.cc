// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/registry.h"
#include "chrome/installer/util/google_update_constants.h"

bool GoogleUpdateSettings::GetCollectStatsConsent() {
  std::wstring reg_path(google_update::kRegPathClientState);
  reg_path.append(L"\\");
  reg_path.append(google_update::kChromeGuid);
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  DWORD value;
  if (!key.ReadValueDW(google_update::kRegUsageStatsField, &value))
    return false;
  return (1 == value);
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  std::wstring reg_path(google_update::kRegPathClientState);
  reg_path.append(L"\\");
  reg_path.append(google_update::kChromeGuid);
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  DWORD value = consented ? 1 : 0;
  return key.WriteValue(google_update::kRegUsageStatsField, value);
}

bool GoogleUpdateSettings::GetBrowser(std::wstring* browser) {
  std::wstring reg_path(google_update::kRegPathClientState);
  reg_path.append(L"\\");
  reg_path.append(google_update::kChromeGuid);
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  return key.ReadValue(google_update::kRegBrowserField, browser);
}

bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  std::wstring reg_path(google_update::kRegPathClientState);
  reg_path.append(L"\\");
  reg_path.append(google_update::kChromeGuid);
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  return key.ReadValue(google_update::kRegLangField, language);
}

bool GoogleUpdateSettings::GetBrand(std::wstring* brand) {
  std::wstring reg_path(google_update::kRegPathClientState);
  reg_path.append(L"\\");
  reg_path.append(google_update::kChromeGuid);
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  return key.ReadValue(google_update::kRegRLZBrandField, brand);
}

