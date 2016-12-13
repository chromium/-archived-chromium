// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_PROVIDER_H_

#include <set>
#include <string>

#include "chrome/browser/extensions/external_extension_provider.h"

class DictionaryValue;
class Version;

// A specialization of the ExternalExtensionProvider that uses a json file to
// look up which external extensions are registered.
class ExternalPrefExtensionProvider : public ExternalExtensionProvider {
 public:
  explicit ExternalPrefExtensionProvider();
  virtual ~ExternalPrefExtensionProvider();

  // Used only during testing to not use the json file for external extensions,
  // but instead parse a json file specified by the test.
  void SetPreferencesForTesting(std::string json_data_for_testing);

  // ExternalExtensionProvider implementation:
  virtual void VisitRegisteredExtension(
      Visitor* visitor, const std::set<std::string>& ids_to_ignore) const;

  virtual Version* RegisteredVersion(std::string id,
                                     Extension::Location* location) const;
 protected:
  scoped_ptr<DictionaryValue> prefs_;

 private:
  void SetPreferences(ValueSerializer* serializer);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_PROVIDER_H_
