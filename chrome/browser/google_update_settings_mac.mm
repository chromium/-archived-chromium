// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Foundation/Foundation.h>

#include "chrome/installer/util/google_update_settings.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/installer/util/google_update_constants.h"

namespace google_update {

// This is copied from chrome/installer/util/google_update_constants.cc
// Reasons duplication acceptable:
// 1. At the time of this writing, this code is a one-off for the dev release.
// 2. The value of this constant is unlikely to change and even if it does
// that negates the point of reusing it in the Mac version.
// 3. To make x-platform usage fo the constants in google_update_constants.cc
// we probably want to split them up into windows-only and x-platform strings.
const wchar_t kRegUsageStatsField[] = L"usagestats";

// Declared in a public namespace for testing purposes.
// If pref not set, assume false.
bool GetCollectStatsConsentFromDictionary(NSDictionary* dict) {
  NSString* collect_stats_key = base::SysWideToNSString(
      google_update::kRegUsageStatsField);
  NSNumber* val = [dict objectForKey:collect_stats_key];

  if (![val respondsToSelector:@selector(boolValue)]) {
    return false;
  }

  return ([val boolValue] == YES);
}

}  // namespace google_update

// static
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  // TODO(mac): This value should be read from the Chrome prefs setting.
  // For Dev-relesae purposes, we read this value from the user's
  // defaults.  This allows easy control of the setting from the terminal.
  // To turn stat reporting off, run the following command from the terminal:
  // $ defaults write com.google.Chrome usagestats -bool 'NO'
  NSUserDefaults* std_defaults = [NSUserDefaults standardUserDefaults];
  return google_update::GetCollectStatsConsentFromDictionary(
      [std_defaults dictionaryRepresentation]);
}

// static
bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  NSString* collect_stats_key = base::SysWideToNSString(
        google_update::kRegUsageStatsField);
  NSUserDefaults* std_defaults = [NSUserDefaults standardUserDefaults];
  BOOL val_to_store = consented ? YES : NO;
  [std_defaults setBool:val_to_store forKey:collect_stats_key];
  return [std_defaults synchronize];
}

// static
bool GoogleUpdateSettings::GetBrowser(std::wstring* browser) {
  NOTIMPLEMENTED();
  return false;
}

// static
bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  NOTIMPLEMENTED();
  return false;
}

// static
bool GoogleUpdateSettings::GetBrand(std::wstring* brand) {
  NOTIMPLEMENTED();
  return false;
}

// static
bool GoogleUpdateSettings::GetReferral(std::wstring* referral) {
  NOTIMPLEMENTED();
  return false;
}

// static
bool GoogleUpdateSettings::ClearReferral() {
  NOTIMPLEMENTED();
  return false;
}

