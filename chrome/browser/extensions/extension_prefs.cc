// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_prefs.h"

#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"

namespace {

// Preferences keys

// A preference that keeps track of per-extension settings. This is a dictionary
// object read from the Preferences file, keyed off of extension id's.
const wchar_t kExtensionsPref[] = L"extensions.settings";

// Where an extension was installed from. (see Extension::Location)
const wchar_t kPrefLocation[] = L"location";

// Enabled, disabled, killed, etc. (see Extension::State)
const wchar_t kPrefState[] = L"state";

// The path to the current version's manifest file.
const wchar_t kPrefPath[] = L"path";

// A preference that tracks extension shelf configuration.  This is a list
// object read from the Preferences file, containing a list of toolstrip URLs.
const wchar_t kExtensionShelf[] = L"extensions.shelf";
}

////////////////////////////////////////////////////////////////////////////////

InstalledExtensions::InstalledExtensions(ExtensionPrefs* prefs) {
  extension_data_.reset(prefs->CopyCurrentExtensions());
}

InstalledExtensions::~InstalledExtensions() {
}

void InstalledExtensions::VisitInstalledExtensions(
    InstalledExtensions::Callback *callback) {
  DictionaryValue::key_iterator extension_id = extension_data_->begin_keys();
  for (; extension_id != extension_data_->end_keys(); ++extension_id) {
    DictionaryValue* ext;
    if (!extension_data_->GetDictionary(*extension_id, &ext)) {
      LOG(WARNING) << "Invalid pref for extension " << *extension_id;
      NOTREACHED();
      continue;
    }
    FilePath::StringType path;
    if (!ext->GetString(kPrefPath, &path)) {
      LOG(WARNING) << "Missing path pref for extension " << *extension_id;
      NOTREACHED();
      continue;
    }
    int location_value;
    if (!ext->GetInteger(kPrefLocation, &location_value)) {
      LOG(WARNING) << "Missing location pref for extension " << *extension_id;
      NOTREACHED();
      continue;
    }
    Extension::Location location =
        static_cast<Extension::Location>(location_value);
    callback->Run(WideToASCII(*extension_id), FilePath(path), location);
  }
}

////////////////////////////////////////////////////////////////////////////////

ExtensionPrefs::ExtensionPrefs(PrefService* prefs) : prefs_(prefs) {
  if (!prefs_->FindPreference(kExtensionsPref))
    prefs_->RegisterDictionaryPref(kExtensionsPref);
  if (!prefs->FindPreference(kExtensionShelf))
    prefs->RegisterListPref(kExtensionShelf);
}

DictionaryValue* ExtensionPrefs::CopyCurrentExtensions() {
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (extensions) {
    DictionaryValue* copy =
        static_cast<DictionaryValue*>(extensions->DeepCopy());
    return copy;
  }
  return new DictionaryValue;
}

void ExtensionPrefs::GetKilledExtensionIds(std::set<std::string>* killed_ids) {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict || dict->GetSize() == 0)
    return;

  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    std::wstring key_name = *i;
    if (!Extension::IdIsValid(WideToASCII(key_name))) {
      LOG(WARNING) << "Invalid external extension ID encountered: "
                   << WideToASCII(key_name);
      continue;
    }

    DictionaryValue* extension = NULL;
    if (!dict->GetDictionary(key_name, &extension)) {
      NOTREACHED();
      continue;
    }

    // Check to see if the extension has been killed.
    int state;
    if (extension->GetInteger(kPrefState, &state) &&
        state == static_cast<int>(Extension::KILLBIT)) {
      StringToLowerASCII(&key_name);
      killed_ids->insert(WideToASCII(key_name));
    }
  }
}

ExtensionPrefs::URLList ExtensionPrefs::GetShelfToolstripOrder() {
  URLList urls;
  const ListValue* toolstrip_urls = prefs_->GetList(kExtensionShelf);
  if (toolstrip_urls) {
    for (size_t i = 0; i < toolstrip_urls->GetSize(); ++i) {
      std::string url;
      if (toolstrip_urls->GetString(i, &url))
        urls.push_back(GURL(url));
    }
  }
  return urls;
}

void ExtensionPrefs::OnExtensionInstalled(Extension* extension) {
  std::string id = extension->id();
  UpdateExtensionPref(id, kPrefState,
                      Value::CreateIntegerValue(Extension::ENABLED));
  UpdateExtensionPref(id, kPrefLocation,
                      Value::CreateIntegerValue(extension->location()));
  UpdateExtensionPref(id, kPrefPath,
                      Value::CreateStringValue(extension->path().value()));
  prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionPrefs::OnExtensionUninstalled(const Extension* extension) {
  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore.
  if (Extension::IsExternalLocation(extension->location())) {
    UpdateExtensionPref(extension->id(), kPrefState,
                        Value::CreateIntegerValue(Extension::KILLBIT));
    prefs_->ScheduleSavePersistentPrefs();
  } else {
    DeleteExtensionPrefs(extension->id());
  }
}

bool ExtensionPrefs::UpdateExtensionPref(const std::string& extension_id,
                                         const std::wstring& key,
                                         Value* data_value) {
  DictionaryValue* extension = GetOrCreateExtensionPref(extension_id);
  if (!extension->Set(key, data_value)) {
    NOTREACHED() << L"Cannot modify key: '" << key.c_str()
                 << "' for extension: '" << extension_id.c_str() << "'";
    return false;
  }
  return true;
}

void ExtensionPrefs::DeleteExtensionPrefs(const std::string& extension_id) {
  std::wstring id = ASCIIToWide(extension_id);
  DictionaryValue* dict = prefs_->GetMutableDictionary(kExtensionsPref);
  if (dict->HasKey(id)) {
    dict->Remove(id, NULL);
    prefs_->ScheduleSavePersistentPrefs();
  }
}

DictionaryValue* ExtensionPrefs::GetOrCreateExtensionPref(
    const std::string& extension_id) {
  DictionaryValue* dict = prefs_->GetMutableDictionary(kExtensionsPref);
  DictionaryValue* extension = NULL;
  std::wstring id = ASCIIToWide(extension_id);
  if (!dict->GetDictionary(id, &extension)) {
    // Extension pref does not exist, create it.
    extension = new DictionaryValue();
    dict->Set(id, extension);
  }
  return extension;
}

