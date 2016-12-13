// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/options_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

// static
void OptionsUtil::ResetToDefaults(Profile* profile) {
  // TODO(tc): It would be nice if we could generate this list automatically so
  // changes to any of the options pages doesn't require updating this list
  // manually.
  PrefService* prefs = profile->GetPrefs();
  const wchar_t* kUserPrefs[] = {
    prefs::kAcceptLanguages,
    prefs::kAlternateErrorPagesEnabled,
    prefs::kCookieBehavior,
    prefs::kDefaultCharset,
    prefs::kDnsPrefetchingEnabled,
    prefs::kDownloadDefaultDirectory,
    prefs::kDownloadExtensionsToOpen,
    prefs::kFormAutofillEnabled,
    prefs::kHomePage,
    prefs::kHomePageIsNewTabPage,
    prefs::kMixedContentFiltering,
    prefs::kPromptForDownload,
    prefs::kPasswordManagerEnabled,
    prefs::kRestoreOnStartup,
    prefs::kSafeBrowsingEnabled,
    prefs::kSearchSuggestEnabled,
    prefs::kShowHomeButton,
    prefs::kSpellCheckDictionary,
    prefs::kURLsToRestoreOnStartup,
    prefs::kWebKitDefaultFixedFontSize,
    prefs::kWebKitDefaultFontSize,
    prefs::kWebKitFixedFontFamily,
    prefs::kWebKitJavaEnabled,
    prefs::kWebKitJavascriptEnabled,
    prefs::kWebKitLoadsImagesAutomatically,
    prefs::kWebKitPluginsEnabled,
    prefs::kWebKitSansSerifFontFamily,
    prefs::kWebKitSerifFontFamily,
  };
  for (size_t i = 0; i < arraysize(kUserPrefs); ++i)
    prefs->ClearPref(kUserPrefs[i]);

  PrefService* local_state = g_browser_process->local_state();
  // Note that we don't reset the kMetricsReportingEnabled preference here
  // because the reset will reset it to the default setting specified in Chrome
  // source, not the default setting selected by the user on the web page where
  // they downloaded Chrome. This means that if the user ever resets their
  // settings they'll either inadvertedly enable this logging or disable it.
  // One is undesirable for them, one is undesirable for us. For now, we just
  // don't reset it.
  const wchar_t* kLocalStatePrefs[] = {
    prefs::kApplicationLocale,
    prefs::kOptionsWindowLastTabIndex,
  };
  for (size_t i = 0; i < arraysize(kLocalStatePrefs); ++i)
    local_state->ClearPref(kLocalStatePrefs[i]);
}
