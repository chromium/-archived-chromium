// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/registry.h"

namespace {

const wchar_t kRegistryBase[] =
    L"Software\\Google\\Update\\ClientState\\{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kUsageStatsFlag[] = L"usagestats";
const wchar_t kBrowserUsed[] = L"browser";
const wchar_t kSelectedLang[] = L"lang";
const wchar_t kRLZBrand[] = L"brand";

}  // namespace

bool GoogleUpdateSettings::GetCollectStatsConsent() {
  RegKey key(HKEY_CURRENT_USER, kRegistryBase, KEY_READ);
  DWORD value;
  if (!key.ReadValueDW(kUsageStatsFlag, &value))
    return false;
  return (1 == value);
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  RegKey key(HKEY_CURRENT_USER, kRegistryBase, KEY_READ | KEY_WRITE);
  DWORD value = consented ? 1 : 0;
  return key.WriteValue(kUsageStatsFlag, value);
}

bool GoogleUpdateSettings::GetBrowser(std::wstring* browser) {
  RegKey key(HKEY_CURRENT_USER, kRegistryBase, KEY_READ);
  return key.ReadValue(kBrowserUsed, browser);
}

bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  RegKey key(HKEY_CURRENT_USER, kRegistryBase, KEY_READ);
  return key.ReadValue(kSelectedLang, language);
}

bool GoogleUpdateSettings::GetBrand(std::wstring* brand) {
  RegKey key(HKEY_CURRENT_USER, kRegistryBase, KEY_READ);
  return key.ReadValue(kRLZBrand, brand);
}

