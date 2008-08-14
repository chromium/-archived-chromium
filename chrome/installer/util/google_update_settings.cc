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
