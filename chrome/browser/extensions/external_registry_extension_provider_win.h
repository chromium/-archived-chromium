// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_EXTENSION_PROVIDER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_EXTENSION_PROVIDER_WIN_H_

#include <set>
#include <string>

#include "chrome/browser/extensions/external_extension_provider.h"

class Version;

// A specialization of the ExternalExtensionProvider that uses the Registry to
// look up which external extensions are registered.
class ExternalRegistryExtensionProvider : public ExternalExtensionProvider {
 public:
  ExternalRegistryExtensionProvider();
  virtual ~ExternalRegistryExtensionProvider();

  // ExternalExtensionProvider implementation:
  virtual void VisitRegisteredExtension(
      Visitor* visitor, const std::set<std::string>& ids_to_ignore) const;

  virtual Version* RegisteredVersion(std::string id,
                                     Extension::Location* location) const;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_EXTENSION_PROVIDER_WIN_H_
