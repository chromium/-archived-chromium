// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_registry_extension_provider_win.h"

#include "base/file_path.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/version.h"

// The Registry hive where to look for external extensions.
const HKEY kRegRoot = HKEY_LOCAL_MACHINE;

// The Registry subkey that contains information about external extensions.
const char kRegistryExtensions[] = "Software\\Google\\Chrome\\Extensions";

// Registry value of of that key that defines the path to the .crx file.
const wchar_t kRegistryExtensionPath[] = L"path";

// Registry value of that key that defines the current version of the .crx file.
const wchar_t kRegistryExtensionVersion[] = L"version";

ExternalRegistryExtensionProvider::ExternalRegistryExtensionProvider() {
}

ExternalRegistryExtensionProvider::~ExternalRegistryExtensionProvider() {
}

void ExternalRegistryExtensionProvider::VisitRegisteredExtension(
    Visitor* visitor, const std::set<std::string>& ids_to_ignore) const {
  RegistryKeyIterator iterator(kRegRoot,
                               ASCIIToWide(kRegistryExtensions).c_str());
  while (iterator.Valid()) {
    RegKey key;
    std::wstring key_path = ASCIIToWide(kRegistryExtensions);
    key_path.append(L"\\");
    key_path.append(iterator.Name());
    if (key.Open(kRegRoot, key_path.c_str())) {
      std::wstring extension_path;
      if (key.ReadValue(kRegistryExtensionPath, &extension_path)) {
        std::wstring extension_version;
        if (key.ReadValue(kRegistryExtensionVersion, &extension_version)) {
          std::string id = WideToASCII(iterator.Name());
          StringToLowerASCII(&id);
          if (ids_to_ignore.find(id) != ids_to_ignore.end()) {
            ++iterator;
            continue;
          }

          scoped_ptr<Version> version;
          version.reset(Version::GetVersionFromString(extension_version));
          FilePath path = FilePath::FromWStringHack(extension_path);
          visitor->OnExternalExtensionFound(id, version.get(), path);
        } else {
          // TODO(erikkay): find a way to get this into about:extensions
          LOG(WARNING) << "Missing value " << kRegistryExtensionVersion <<
                          " for key " << key_path;
        }
      } else {
        // TODO(erikkay): find a way to get this into about:extensions
        LOG(WARNING) << "Missing value " << kRegistryExtensionPath <<
                        " for key " << key_path;
      }
    }
    ++iterator;
  }
}

Version* ExternalRegistryExtensionProvider::RegisteredVersion(
    std::string id,
    Extension::Location* location) const  {
  RegKey key;
  std::wstring key_path = ASCIIToWide(kRegistryExtensions);
  key_path.append(L"\\");
  key_path.append(ASCIIToWide(id));

  if (!key.Open(kRegRoot, key_path.c_str()))
    return NULL;

  std::wstring extension_version;
  if (!key.ReadValue(kRegistryExtensionVersion, &extension_version))
    return NULL;

  if (location)
    *location = Extension::EXTERNAL_REGISTRY;
  return Version::GetVersionFromString(extension_version);
}
