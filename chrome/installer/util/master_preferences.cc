// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/master_preferences.h"

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

bool GetBooleanPref(const DictionaryValue* prefs, const std::wstring& name) {
  bool value = false;
  prefs->GetBoolean(name, &value);
  return value;
}

}  // namespace

namespace installer_util {

// Boolean pref that triggers skipping the first run dialogs.
const wchar_t kDistroSkipFirstRunPref[] = L"distribution.skip_first_run_ui";
// Boolean pref that triggers loading the welcome page.
const wchar_t kDistroShowWelcomePage[] = L"distribution.show_welcome_page";
// Boolean pref that triggers silent import of the default search engine.
const wchar_t kDistroImportSearchPref[] = L"distribution.import_search_engine";
// Boolean pref that triggers silent import of the browse history.
const wchar_t kDistroImportHistoryPref[] = L"distribution.import_history";
// The following boolean prefs have the same semantics as the corresponding
// setup command line switches. See chrome/installer/util/util_constants.cc
// for more info.
// Create Desktop and QuickLaunch shortcuts.
const wchar_t kCreateAllShortcuts[] = L"distribution.create_all_shortcuts";
// Prevent installer from launching Chrome after a successful first install.
const wchar_t kDoNotLaunchChrome[] = L"distribution.do_not_launch_chrome";
// Register Chrome as default browser on the system.
const wchar_t kMakeChromeDefault[] = L"distribution.make_chrome_default";
// Install Chrome to system wise location.
const wchar_t kSystemLevel[] = L"distribution.system_level";
// Run installer in verbose mode.
const wchar_t kVerboseLogging[] = L"distribution.verbose_logging";
// Show EULA dialog and install only if accepted.
const wchar_t kRequireEula[] = L"distribution.require_eula";


int ParseDistributionPreferences(const std::wstring& master_prefs_path) {

  if (!file_util::PathExists(master_prefs_path))
    return MASTER_PROFILE_NOT_FOUND;

  LOG(INFO) << "master profile found";

  std::string json_data;
  if (!file_util::ReadFileToString(master_prefs_path, &json_data))
    return MASTER_PROFILE_ERROR;

  scoped_ptr<DictionaryValue> json_root(ReadJSONPrefs(json_data));
  if (!json_root.get())
    return MASTER_PROFILE_ERROR;

  int parse_result = 0;

  if (GetBooleanPref(json_root.get(), kDistroSkipFirstRunPref))
    parse_result |= MASTER_PROFILE_NO_FIRST_RUN_UI;

  if (GetBooleanPref(json_root.get(), kDistroShowWelcomePage))
    parse_result |= MASTER_PROFILE_SHOW_WELCOME;

  if (GetBooleanPref(json_root.get(), kDistroImportSearchPref))
    parse_result |= MASTER_PROFILE_IMPORT_SEARCH_ENGINE;

  if (GetBooleanPref(json_root.get(), kDistroImportHistoryPref))
    parse_result |= MASTER_PROFILE_IMPORT_HISTORY;

  if (GetBooleanPref(json_root.get(), kCreateAllShortcuts))
    parse_result |= MASTER_PROFILE_CREATE_ALL_SHORTCUTS;

  if (GetBooleanPref(json_root.get(), kDoNotLaunchChrome))
    parse_result |= MASTER_PROFILE_DO_NOT_LAUNCH_CHROME;

  if (GetBooleanPref(json_root.get(), kMakeChromeDefault))
    parse_result |= MASTER_PROFILE_MAKE_CHROME_DEFAULT;

  if (GetBooleanPref(json_root.get(), kSystemLevel))
    parse_result |= MASTER_PROFILE_SYSTEM_LEVEL;

  if (GetBooleanPref(json_root.get(), kVerboseLogging))
    parse_result |= MASTER_PROFILE_VERBOSE_LOGGING;

  if (GetBooleanPref(json_root.get(), kRequireEula))
    parse_result |= MASTER_PROFILE_REQUIRE_EULA;

  return parse_result;
}

}  // installer_util
