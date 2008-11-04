// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains functions processing master preference file used by
// setup and first run.

#ifndef CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H__
#define CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H__

#include <string>

namespace installer_util {

// This is the default name for the master preferences file used to pre-set
// values in the user profile at first run.
const wchar_t kDefaultMasterPrefs[] = L"master_preferences";

// Boolean pref that triggers skipping the first run dialogs.
const wchar_t kDistroSkipFirstRunPref[] = L"distribution.skip_first_run_ui";
// Boolean pref that triggers loading the welcome page.
const wchar_t kDistroShowWelcomePage[] = L"distribution.show_welcome_page";
// Boolean pref that triggers silent import of the default search engine.
const wchar_t kDistroImportSearchPref[] = L"distribution.import_search_engine";
// Boolean pref that triggers silent import of the browse history.
const wchar_t kDistroImportHistoryPref[] = L"distribution.import_history";

// These are the possible results of calling ParseDistributionPreferences.
// Some of the results can be combined, so they are bit flags.
enum MasterPrefResult {
  MASTER_PROFILE_NOT_FOUND            = 0x1,
  // A critical error processing the master profile.
  MASTER_PROFILE_ERROR                = 0x1 << 1,
  // Skip first run dialogs.
  MASTER_PROFILE_NO_FIRST_RUN_UI      = 0x1 << 2,
  // Show welcome page.
  MASTER_PROFILE_SHOW_WELCOME         = 0x1 << 3,
  // Improt search engine setting from the default browser.
  MASTER_PROFILE_IMPORT_SEARCH_ENGINE = 0x1 << 4,
  // Improt history from the default browser.
  MASTER_PROFILE_IMPORT_HISTORY       = 0x1 << 5,
};

// The master preferences is a JSON file with the same entries as the
// 'Default\Preferences' file. This function parses the distribution
// section of the preferences file.
//
// A prototypical 'master_preferences' file looks like this:
//
// {
//   "distribution": {
//      "skip_first_run_ui": true,
//      "show_welcome_page": true,
//      "import_search_engine": true,
//      "import_history": false
//   },
//   "browser": {
//      "show_home_button": true
//   },
//   "bookmark_bar": {
//      "show_on_all_tabs": true
//   },
//   "homepage": "http://example.org",
//   "homepage_is_newtabpage": false
// }
//
// A reserved "distribution" entry in the file is used to group related
// installation properties. This entry will be ignored at other times.
// This function parses the 'distribution' entry and returns a combination
// of MasterPrefResult.
int ParseDistributionPreferences(const std::wstring& master_prefs_path);

}

#endif  // CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H__
