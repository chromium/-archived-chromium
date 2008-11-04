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
  Value* root;
  if (!json.Deserialize(&root))
    return NULL;
  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    delete root;
    return NULL;
  }
  return static_cast<DictionaryValue*>(root);
}

bool GetBooleanPref(const DictionaryValue* prefs, const std::wstring& name) {
  bool value = false;
  prefs->GetBoolean(name, &value);
  return value;
}

}  // namespace

namespace installer_util {

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

  return parse_result;
}

}  // installer_util
