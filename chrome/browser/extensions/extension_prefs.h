// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H

#include <set>
#include <string>
#include <vector>

#include "base/task.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"

// Class for managing global and per-extension preferences.
// This class is instantiated by ExtensionsService, so it should be accessed
// from there.
class ExtensionPrefs {
 public:
  explicit ExtensionPrefs(PrefService* prefs, const FilePath& root_dir_);

  // Returns a copy of the Extensions prefs.
  // TODO(erikkay) Remove this so that external consumers don't need to be
  // aware of the internal structure of the preferences.
  DictionaryValue* CopyCurrentExtensions();

  // Populate |killed_ids| with extension ids that have been killed.
  void GetKilledExtensionIds(std::set<std::string>* killed_ids);

  // Get the order that toolstrip URLs appear in the shelf.
  typedef std::vector<GURL> URLList;
  URLList GetShelfToolstripOrder();

  // Sets the order that toolstrip URLs appear in the shelf.
  void SetShelfToolstripOrder(const URLList& urls);

  // Called when an extension is installed, so that prefs get created.
  void OnExtensionInstalled(Extension* extension);

  // Called when an extension is uninstalled, so that prefs get cleaned up.
  void OnExtensionUninstalled(const Extension* extension,
                              bool external_uninstall);

  // Returns base extensions install directory.
  const FilePath& install_directory() const { return install_directory_; }

 private:

  // Converts absolute paths in the pref to paths relative to the
  // install_directory_.
  void MakePathsRelative();

  // Converts internal relative paths to be absolute. Used for export to
  // consumers who expect full paths.
  void MakePathsAbsolute(DictionaryValue* dict);

  // Sets the pref |key| for extension |id| to |value|.
  bool UpdateExtensionPref(const std::string& id,
                           const std::wstring& key,
                           Value* value);

  // Deletes the pref dictionary for extension |id|.
  void DeleteExtensionPrefs(const std::string& id);

  // Ensures and returns a mutable dictionary for extension |id|'s prefs.
  DictionaryValue* GetOrCreateExtensionPref(const std::string& id);

  // The pref service specific to this set of extension prefs.
  PrefService* prefs_;

  // Base extensions install directory.
  FilePath install_directory_;

  // The URLs of all of the toolstrips.
  URLList shelf_order_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefs);
};

// A helper class that has a list of the currently installed extensions
// and can iterate over them to a provided callback.
class InstalledExtensions {
 public:
  explicit InstalledExtensions(ExtensionPrefs* prefs);
  ~InstalledExtensions();

  typedef Callback3<const std::string&,
                    const FilePath&,
                    Extension::Location>::Type Callback;

  // Runs |callback| for each installed extension with the path to the
  // version directory and the location.
  void VisitInstalledExtensions(Callback *callback);

 private:
  // A copy of the extensions pref dictionary so that this can be passed
  // around without a dependency on prefs.
  scoped_ptr<DictionaryValue> extension_data_;

  DISALLOW_COPY_AND_ASSIGN(InstalledExtensions);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_H
