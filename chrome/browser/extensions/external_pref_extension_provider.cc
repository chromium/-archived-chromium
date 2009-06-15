// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_extension_provider.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/version.h"

// Constants for keeping track of extension preferences.
const wchar_t kLocation[] = L"location";
const wchar_t kState[] = L"state";
const wchar_t kExternalCrx[] = L"external_crx";
const wchar_t kExternalVersion[] = L"external_version";

ExternalPrefExtensionProvider::ExternalPrefExtensionProvider(
    DictionaryValue* prefs) {
  DCHECK(prefs);
  prefs_.reset(prefs);
}

ExternalPrefExtensionProvider::~ExternalPrefExtensionProvider() {
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

    int location;
    if (extension->GetInteger(kLocation, &location) &&
        location != Extension::EXTERNAL_PREF) {
      continue;
    }
    int state;
    if (extension->GetInteger(kState, &state) &&
        state == Extension::KILLBIT) {
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

    scoped_ptr<Version> version;
    version.reset(Version::GetVersionFromString(external_version));
    visitor->OnExternalExtensionFound(
        WideToASCII(extension_id), version.get(), FilePath(external_crx));
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
