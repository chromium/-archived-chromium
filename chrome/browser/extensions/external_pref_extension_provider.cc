// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_extension_provider.h"

#include "app/app_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/version.h"
#include "chrome/common/json_value_serializer.h"

// Constants for keeping track of extension preferences.
const wchar_t kLocation[] = L"location";
const wchar_t kState[] = L"state";
const wchar_t kExternalCrx[] = L"external_crx";
const wchar_t kExternalVersion[] = L"external_version";

ExternalPrefExtensionProvider::ExternalPrefExtensionProvider() {
  FilePath json_file;
  PathService::Get(app::DIR_EXTERNAL_EXTENSIONS, &json_file);
  json_file = json_file.Append(FILE_PATH_LITERAL("external_extensions.json"));

  JSONFileValueSerializer serializer(json_file);
  SetPreferences(&serializer);
}

ExternalPrefExtensionProvider::~ExternalPrefExtensionProvider() {
}

void ExternalPrefExtensionProvider::SetPreferencesForTesting(
    std::string json_data_for_testing) {
  JSONStringValueSerializer serializer(json_data_for_testing);
  SetPreferences(&serializer);
}

void ExternalPrefExtensionProvider::VisitRegisteredExtension(
    Visitor* visitor, const std::set<std::string>& ids_to_ignore) const {
  for (DictionaryValue::key_iterator i = prefs_->begin_keys();
       i != prefs_->end_keys(); ++i) {
    const std::wstring& extension_id = *i;
    if (ids_to_ignore.find(WideToASCII(extension_id)) != ids_to_ignore.end())
      continue;

    DictionaryValue* extension = NULL;
    if (!prefs_->GetDictionary(extension_id, &extension)) {
      continue;
    }

    FilePath::StringType external_crx;
    std::string external_version;
    if (!extension->GetString(kExternalCrx, &external_crx) ||
        !extension->GetString(kExternalVersion, &external_version)) {
      LOG(WARNING) << "Malformed extension dictionary for extension: "
                   << extension_id.c_str();
      continue;
    }

    if (external_crx.find(FilePath::kParentDirectory) != StringPiece::npos) {
      LOG(WARNING) << "Path traversal not allowed in path: "
                   << external_crx.c_str();
      continue;
    }

    // See if it's an absolute path...
    FilePath path(external_crx);
    if (!path.IsAbsolute()) {
      // Try path as relative path from external extension dir.
      FilePath base_path;
      PathService::Get(app::DIR_EXTERNAL_EXTENSIONS, &base_path);
      path = base_path.Append(external_crx);
    }

    scoped_ptr<Version> version;
    version.reset(Version::GetVersionFromString(external_version));
    visitor->OnExternalExtensionFound(
        WideToASCII(extension_id), version.get(), path);
  }
}

Version* ExternalPrefExtensionProvider::RegisteredVersion(
    std::string id, Extension::Location* location) const {
  DictionaryValue* extension = NULL;
  if (!prefs_->GetDictionary(ASCIIToWide(id), &extension))
    return NULL;

  std::string external_version;
  if (!extension->GetString(kExternalVersion, &external_version))
    return NULL;

  if (location)
    *location = Extension::EXTERNAL_PREF;
  return Version::GetVersionFromString(external_version);
}

void ExternalPrefExtensionProvider::SetPreferences(
    ValueSerializer* serializer) {
  std::string error_msg;
  Value* extensions = serializer->Deserialize(&error_msg);
  scoped_ptr<DictionaryValue> dictionary(new DictionaryValue());
  if (!error_msg.empty()) {
    LOG(WARNING) << L"Unable to deserialize json data: "
                 << error_msg.c_str();
  } else {
    // This can be null if the json file specified does not exist.
    if (extensions) {
      if (!extensions->IsType(Value::TYPE_DICTIONARY)) {
        NOTREACHED() << L"Invalid json data";
      } else {
        dictionary.reset(static_cast<DictionaryValue*>(extensions));
      }
    }
  }
  prefs_.reset(dictionary.release());
}
