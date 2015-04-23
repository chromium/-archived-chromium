// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/master_preferences.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/json_value_serializer.h"

namespace {

DictionaryValue* ReadJSONPrefs(const std::string& data) {
  JSONStringValueSerializer json(data);
  scoped_ptr<Value> root(json.Deserialize(NULL));
  if (!root.get())
    return NULL;
  if (!root->IsType(Value::TYPE_DICTIONARY))
    return NULL;

  return static_cast<DictionaryValue*>(root.release());
}

DictionaryValue* GetPrefsFromFile(const std::wstring& master_prefs_path) {
  std::string json_data;
  if (!file_util::ReadFileToString(master_prefs_path, &json_data))
    return NULL;
  return ReadJSONPrefs(json_data);
}

bool GetBooleanPref(const DictionaryValue* prefs, const std::wstring& name) {
  bool value = false;
  prefs->GetBoolean(name, &value);
  return value;
}

}  // namespace

namespace installer_util {
// All the preferences below are expected to be inside the JSON "distribution"
// block. See master_preferences.h for an example.

// Boolean pref that triggers skipping the first run dialogs.
const wchar_t kDistroSkipFirstRunPref[] = L"skip_first_run_ui";
// Boolean pref that triggers loading the welcome page.
const wchar_t kDistroShowWelcomePage[] = L"show_welcome_page";
// Boolean pref that triggers silent import of the default search engine.
const wchar_t kDistroImportSearchPref[] = L"import_search_engine";
// Boolean pref that triggers silent import of the default browser history.
const wchar_t kDistroImportHistoryPref[] = L"import_history";
// Boolean pref that triggers silent import of the default browser bookmarks.
const wchar_t kDistroImportBookmarksPref[] = L"import_bookmarks";
// RLZ ping delay in seconds
const wchar_t kDistroPingDelay[] = L"ping_delay";
// Register Chrome as default browser for the current user.
const wchar_t kMakeChromeDefaultForUser[] = L"make_chrome_default_for_user";
// The following boolean prefs have the same semantics as the corresponding
// setup command line switches. See chrome/installer/util/util_constants.cc
// for more info.
// Create Desktop and QuickLaunch shortcuts.
const wchar_t kCreateAllShortcuts[] = L"create_all_shortcuts";
// Prevent installer from launching Chrome after a successful first install.
const wchar_t kDoNotLaunchChrome[] = L"do_not_launch_chrome";
// Register Chrome as default browser on the system.
const wchar_t kMakeChromeDefault[] = L"make_chrome_default";
// Install Chrome to system wise location.
const wchar_t kSystemLevel[] = L"system_level";
// Run installer in verbose mode.
const wchar_t kVerboseLogging[] = L"verbose_logging";
// Show EULA dialog and install only if accepted.
const wchar_t kRequireEula[] = L"require_eula";
// Use alternate shortcut text for the main shortcut.
const wchar_t kAltShortcutText[] = L"alternate_shortcut_text";
// Use alternate smaller first run info bubble.
const wchar_t kAltFirstRunBubble[] = L"oem_bubble";
// Boolean pref that triggers silent import of the default browser homepage.
const wchar_t kDistroImportHomePagePref[] = L"import_home_page";

bool GetDistributionPingDelay(const FilePath& master_prefs_path,
                              int& delay) {
  // 90 seconds is the default that we want to use in case master preferences
  // is missing or corrupt.
  delay = 90;
  FilePath master_prefs = master_prefs_path;
  if (master_prefs.empty()) {
    if (!PathService::Get(base::DIR_EXE, &master_prefs))
      return false;
    master_prefs = master_prefs.Append(installer_util::kDefaultMasterPrefs);
  }

  if (!file_util::PathExists(master_prefs))
    return false;

  scoped_ptr<DictionaryValue> json_root(
      GetPrefsFromFile(master_prefs.ToWStringHack()));
  if (!json_root.get())
    return false;

  DictionaryValue* distro = NULL;
  if (!json_root->GetDictionary(L"distribution", &distro) ||
      !distro->GetInteger(kDistroPingDelay, &delay))
    return false;

  return true;
}

int ParseDistributionPreferences(const std::wstring& master_prefs_path) {
  if (!file_util::PathExists(master_prefs_path))
    return MASTER_PROFILE_NOT_FOUND;
  LOG(INFO) << "master profile found";

  scoped_ptr<DictionaryValue> json_root(GetPrefsFromFile(master_prefs_path));
  if (!json_root.get())
    return MASTER_PROFILE_ERROR;

  int parse_result = 0;
  DictionaryValue* distro = NULL;
  if (json_root->GetDictionary(L"distribution", &distro)) {
    if (GetBooleanPref(distro, kDistroSkipFirstRunPref))
      parse_result |= MASTER_PROFILE_NO_FIRST_RUN_UI;
    if (GetBooleanPref(distro, kDistroShowWelcomePage))
      parse_result |= MASTER_PROFILE_SHOW_WELCOME;
    if (GetBooleanPref(distro, kDistroImportSearchPref))
      parse_result |= MASTER_PROFILE_IMPORT_SEARCH_ENGINE;
    if (GetBooleanPref(distro, kDistroImportHistoryPref))
      parse_result |= MASTER_PROFILE_IMPORT_HISTORY;
    if (GetBooleanPref(distro, kDistroImportBookmarksPref))
      parse_result |= MASTER_PROFILE_IMPORT_BOOKMARKS;
    if (GetBooleanPref(distro, kDistroImportHomePagePref))
      parse_result |= MASTER_PROFILE_IMPORT_HOME_PAGE;
    if (GetBooleanPref(distro, kMakeChromeDefaultForUser))
      parse_result |= MASTER_PROFILE_MAKE_CHROME_DEFAULT_FOR_USER;
    if (GetBooleanPref(distro, kCreateAllShortcuts))
      parse_result |= MASTER_PROFILE_CREATE_ALL_SHORTCUTS;
    if (GetBooleanPref(distro, kDoNotLaunchChrome))
      parse_result |= MASTER_PROFILE_DO_NOT_LAUNCH_CHROME;
    if (GetBooleanPref(distro, kMakeChromeDefault))
      parse_result |= MASTER_PROFILE_MAKE_CHROME_DEFAULT;
    if (GetBooleanPref(distro, kSystemLevel))
      parse_result |= MASTER_PROFILE_SYSTEM_LEVEL;
    if (GetBooleanPref(distro, kVerboseLogging))
      parse_result |= MASTER_PROFILE_VERBOSE_LOGGING;
    if (GetBooleanPref(distro, kRequireEula))
      parse_result |= MASTER_PROFILE_REQUIRE_EULA;
    if (GetBooleanPref(distro, kAltShortcutText))
      parse_result |= MASTER_PROFILE_ALT_SHORTCUT_TXT;
    if (GetBooleanPref(distro, kAltFirstRunBubble))
      parse_result |= MASTER_PROFILE_OEM_FIRST_RUN_BUBBLE;
  }
  return parse_result;
}

std::vector<std::wstring> ParseFirstRunTabs(
    const std::wstring& master_prefs_path) {
  std::vector<std::wstring> launch_tabs;
  scoped_ptr<DictionaryValue> json_root(GetPrefsFromFile(master_prefs_path));
  if (!json_root.get())
    return launch_tabs;
  ListValue* tabs_list = NULL;
  if (!json_root->GetList(L"first_run_tabs", &tabs_list))
    return launch_tabs;
  for (size_t i = 0; i < tabs_list->GetSize(); ++i) {
    Value* entry;
    std::wstring tab_entry;
    if (!tabs_list->Get(i, &entry) || !entry->GetAsString(&tab_entry)) {
      NOTREACHED();
      break;
    }
    launch_tabs.push_back(tab_entry);
  }
  return launch_tabs;
}

}  // installer_util
