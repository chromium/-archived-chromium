// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains functions processing master preference file used by
// setup and first run.

#ifndef CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
#define CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_

#include <string>
#include <vector>

#include "base/file_util.h"

namespace installer_util {

// This is the default name for the master preferences file used to pre-set
// values in the user profile at first run.
const wchar_t kDefaultMasterPrefs[] = L"master_preferences";

// These are the possible results of calling ParseDistributionPreferences.
// Some of the results can be combined, so they are bit flags.
enum MasterPrefResult {
  MASTER_PROFILE_NOT_FOUND                     = 0x1,
  // A critical error processing the master profile.
  MASTER_PROFILE_ERROR                         = 0x1 << 1,
  // Skip first run dialogs.
  MASTER_PROFILE_NO_FIRST_RUN_UI               = 0x1 << 2,
  // Show welcome page.
  MASTER_PROFILE_SHOW_WELCOME                  = 0x1 << 3,
  // Import search engine setting from the default browser.
  MASTER_PROFILE_IMPORT_SEARCH_ENGINE          = 0x1 << 4,
  // Import history from the default browser.
  MASTER_PROFILE_IMPORT_HISTORY                = 0x1 << 5,
  // Import bookmarks from the default browser.
  MASTER_PROFILE_IMPORT_BOOKMARKS              = 0x1 << 6,
  // Register Chrome as default browser for the current user. This option is
  // different than MAKE_CHROME_DEFAULT as installer ignores this option and
  // Chrome on first run makes itself default.
  MASTER_PROFILE_MAKE_CHROME_DEFAULT_FOR_USER  = 0x1 << 7,
  // The following boolean prefs have the same semantics as the corresponding
  // setup command line switches. See chrome/installer/util/util_constants.cc
  // for more info.
  // Create Desktop and QuickLaunch shortcuts.
  MASTER_PROFILE_CREATE_ALL_SHORTCUTS          = 0x1 << 8,
  // Prevent installer from launching Chrome after a successful first install.
  MASTER_PROFILE_DO_NOT_LAUNCH_CHROME          = 0x1 << 9,
  // Register Chrome as default browser on the system.
  MASTER_PROFILE_MAKE_CHROME_DEFAULT           = 0x1 << 10,
  // Install Chrome to system wise location.
  MASTER_PROFILE_SYSTEM_LEVEL                  = 0x1 << 11,
  // Run installer in verbose mode.
  MASTER_PROFILE_VERBOSE_LOGGING               = 0x1 << 12,
  // Show the EULA and do not install if not accepted.
  MASTER_PROFILE_REQUIRE_EULA                  = 0x1 << 13,
  // Use an alternate description text for some shortcuts.
  MASTER_PROFILE_ALT_SHORTCUT_TXT              = 0x1 << 14,
  // Use a smaller OEM info bubble on first run.
  MASTER_PROFILE_OEM_FIRST_RUN_BUBBLE          = 0x1 << 15,
  // Import home page from the default browser.
  MASTER_PROFILE_IMPORT_HOME_PAGE              = 0x1 << 16
};

// This function gets ping delay (ping_delay in the sample above) from master
// preferences.
bool GetDistributionPingDelay(const FilePath& master_prefs_path,
                              int& delay);

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
//      "import_bookmarks": false,
//      "import_home_page": false,
//      "create_all_shortcuts": true,
//      "do_not_launch_chrome": false,
//      "make_chrome_default": false,
//      "make_chrome_default_for_user": true,
//      "system_level": false,
//      "verbose_logging": true,
//      "require_eula": true,
//      "alternate_shortcut_text": false,
//      "ping_delay": 40
//   },
//   "browser": {
//      "show_home_button": true
//   },
//   "bookmark_bar": {
//      "show_on_all_tabs": true
//   },
//   "first_run_tabs": [
//      "http://gmail.com",
//      "https://igoogle.com"
//   ],
//   "homepage": "http://example.org",
//   "homepage_is_newtabpage": false
// }
//
// A reserved "distribution" entry in the file is used to group related
// installation properties. This entry will be ignored at other times.
// This function parses the 'distribution' entry and returns a combination
// of MasterPrefResult.
int ParseDistributionPreferences(const std::wstring& master_prefs_path);

// As part of the master preferences an optional section indicates the tabs
// to open during first run. An example is the following:
//
//  {
//    "first_run_tabs": [
//       "http://google.com/f1",
//       "https://google.com/f2"
//    ]
//  }
//
// Note that the entries are usually urls but they don't have to.
//
// This function retuns the list as a vector of strings. If the master
// preferences file does not contain such list the vector is empty.
std::vector<std::wstring> ParseFirstRunTabs(
    const std::wstring& master_prefs_path);
}

#endif  // CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
