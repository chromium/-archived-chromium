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
  // Import search engine setting from the default browser.
  MASTER_PROFILE_IMPORT_SEARCH_ENGINE = 0x1 << 4,
  // Import history from the default browser.
  MASTER_PROFILE_IMPORT_HISTORY       = 0x1 << 5,
  // The following boolean prefs have the same semantics as the corresponding
  // setup command line switches. See chrome/installer/util/util_constants.cc
  // for more info.
  // Create Desktop and QuickLaunch shortcuts.
  MASTER_PROFILE_CREATE_ALL_SHORTCUTS = 0x1 << 6,
  // Prevent installer from launching Chrome after a successful first install.
  MASTER_PROFILE_DO_NOT_LAUNCH_CHROME = 0x1 << 7,
  // Register Chrome as default browser on the system.
  MASTER_PROFILE_MAKE_CHROME_DEFAULT  = 0x1 << 8,
  // Install Chrome to system wise location.
  MASTER_PROFILE_SYSTEM_LEVEL         = 0x1 << 9,
  // Run installer in verbose mode.
  MASTER_PROFILE_VERBOSE_LOGGING      = 0x1 << 10,
  // Show the EULA and do not install if not accepted.
  MASTER_PROFILE_REQUIRE_EULA         = 0x1 << 11
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
//      "import_history": false,
//      "create_all_shortcuts": true,
//      "do_not_launch_chrome": false,
//      "make_chrome_default": false,
//      "system_level": false,
//      "verbose_logging": true,
//      "require_eula": true
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
