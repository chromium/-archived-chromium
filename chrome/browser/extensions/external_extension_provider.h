// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_H_

#include <set>
#include <string>

#include "base/version.h"
#include "chrome/common/extensions/extension.h"

class FilePath;

// This class is an abstract class for implementing external extensions
// providers.
class ExternalExtensionProvider {
 public:
  // ExternalExtensionProvider uses this interface to communicate back to the
  // caller what extensions are registered, and which |id|, |version| and |path|
  // they have. See also VisitRegisteredExtension below. Ownership of |version|
  // is not transferred to the visitor.
  class Visitor {
   public:
     virtual void OnExternalExtensionFound(const std::string& id,
                                           const Version* version,
                                           const FilePath& path) = 0;
  };

  virtual ~ExternalExtensionProvider() {}

  // Enumerate registered extension, calling OnExternalExtensionFound on
  // the |visitor| object for each registered extension found. |ids_to_ignore|
  // contains a list of extension ids that should not result in a call back.
  virtual void VisitRegisteredExtension(
      Visitor* visitor, const std::set<std::string>& ids_to_ignore) const = 0;

  // Gets the version of extension with |id| and its |location|. |location| can
  // be NULL. The caller is responsible for cleaning up the Version object
  // returned. This function returns NULL if the extension is not found.
  virtual Version* RegisteredVersion(std::string id,
                                     Extension::Location* location) const = 0;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_H_
