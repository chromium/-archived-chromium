// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_H_

#include <string>
#include <vector>
#include <map>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/page_action.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkColor.h"

// Represents a Chromium extension.
class Extension {
 public:
  // What an extension was loaded from.
  enum Location {
    INVALID,
    INTERNAL,  // A crx file from the internal Extensions directory
    EXTERNAL,  // A crx file from an external directory (via eg the registry
               // on Windows)
    LOAD  // --load-extension 
  };

  // The name of the manifest inside an extension.
  static const char kManifestFilename[];

  // Keys used in JSON representation of extensions.
  static const wchar_t* kContentScriptsKey;
  static const wchar_t* kCssKey;
  static const wchar_t* kDescriptionKey;
  static const wchar_t* kIconPathKey;
  static const wchar_t* kIdKey;
  static const wchar_t* kJsKey;
  static const wchar_t* kMatchesKey;
  static const wchar_t* kNameKey;
  static const wchar_t* kPageActionsKey;
  static const wchar_t* kPermissionsKey;
  static const wchar_t* kPluginsDirKey;
  static const wchar_t* kBackgroundKey;
  static const wchar_t* kRunAtKey;
  static const wchar_t* kThemeKey;
  static const wchar_t* kThemeImagesKey;
  static const wchar_t* kThemeColorsKey;
  static const wchar_t* kThemeTintsKey;
  static const wchar_t* kToolstripsKey;
  static const wchar_t* kTooltipKey;
  static const wchar_t* kTypeKey;
  static const wchar_t* kVersionKey;
  static const wchar_t* kZipHashKey;

  // Some values expected in manifests.
  static const char* kRunAtDocumentStartValue;
  static const char* kRunAtDocumentEndValue;
  static const char* kPageActionTypeTab;
  static const char* kPageActionTypePermanent;

  // Error messages returned from InitFromValue().
  static const char* kInvalidContentScriptError;
  static const char* kInvalidContentScriptsListError;
  static const char* kInvalidCssError;
  static const char* kInvalidCssListError;
  static const char* kInvalidDescriptionError;
  static const char* kInvalidIdError;
  static const char* kInvalidJsError;
  static const char* kInvalidJsListError;
  static const char* kInvalidManifestError;
  static const char* kInvalidMatchCountError;
  static const char* kInvalidMatchError;
  static const char* kInvalidMatchesError;
  static const char* kInvalidNameError;
  static const char* kInvalidPluginsDirError;
  static const char* kInvalidBackgroundError;
  static const char* kInvalidRunAtError;
  static const char* kInvalidToolstripError;
  static const char* kInvalidToolstripsError;
  static const char* kInvalidVersionError;
  static const char* kInvalidPageActionError;
  static const char* kInvalidPageActionsListError;
  static const char* kInvalidPageActionIconPathError;
  static const char* kInvalidPageActionTooltipError;
  static const char* kInvalidPageActionTypeValueError;
  static const char* kInvalidPermissionsError;
  static const char* kInvalidPermissionCountWarning;
  static const char* kInvalidPermissionError;
  static const char* kInvalidPermissionSchemeError;
  static const char* kInvalidZipHashError;
  static const char* kInvalidThemeError;
  static const char* kInvalidThemeImagesMissingError;
  static const char* kInvalidThemeImagesError;
  static const char* kInvalidThemeColorsError;
  static const char* kInvalidThemeTintsError;
  static const char* kMissingFileError;
  static const char* kMissingPageActionIcon;

  // The number of bytes in a legal id.
  static const size_t kIdSize;

  Extension() : location_(INVALID) {}
  explicit Extension(const FilePath& path);
  explicit Extension(const Extension& other);
  virtual ~Extension();

  // Returns an absolute url to a resource inside of an extension. The
  // |extension_url| argument should be the url() from an Extension object. The
  // |relative_path| can be untrusted user input. The returned URL will either
  // be invalid() or a child of |extension_url|.
  // NOTE: Static so that it can be used from multiple threads.
  static GURL GetResourceURL(const GURL& extension_url,
                             const std::string& relative_path);
  GURL GetResourceURL(const std::string& relative_path) {
    return GetResourceURL(url(), relative_path);
  }

  // Returns an absolute path to a resource inside of an extension. The
  // |extension_path| argument should be the path() from an Extension object.
  // The |relative_path| can be untrusted user input. The returned path will
  // either be empty or a child of extension_path.
  // NOTE: Static so that it can be used from multiple threads.
  static FilePath GetResourcePath(const FilePath& extension_path,
                                  const std::string& relative_path);
  FilePath GetResourcePath(const std::string& relative_path) {
    return GetResourcePath(path(), relative_path);
  }

  // Initialize the extension from a parsed manifest.
  // If |require_id| is true, will return an error if the "id" key is missing
  // from the value.
  bool InitFromValue(const DictionaryValue& value, bool require_id,
                     std::string* error);

  const FilePath& path() const { return path_; }
  const GURL& url() const { return extension_url_; }
  const Location location() const { return location_; }
  void set_location(Location location) { location_ = location; }
  const std::string& id() const { return id_; }
  const Version* version() const { return version_.get(); }
  // String representation of the version number.
  const std::string VersionString() const;
  const std::string& name() const { return name_; }
  const std::string& description() const { return description_; }
  const UserScriptList& content_scripts() const { return content_scripts_; }
  const PageActionMap& page_actions() const { return page_actions_; }
  const FilePath& plugins_dir() const { return plugins_dir_; }
  const GURL& background_url() const { return background_url_; }
  const std::vector<std::string>& toolstrips() const { return toolstrips_; }
  const std::vector<URLPattern>& permissions() const {
      return permissions_; }

  // Retrieves a page action by |id|.
  const PageAction* GetPageAction(std::string id) const;

  // Theme-related
  DictionaryValue* GetThemeImages() const { return theme_images_.get(); }
  DictionaryValue* GetThemeColors() const { return theme_colors_.get(); }
  DictionaryValue* GetThemeTints() const { return theme_tints_.get(); }
  bool IsTheme() { return is_theme_; }

  // It doesn't really make sense to 'uninstall' extensions loaded from
  // --load-extension or external locations.
  const bool is_uninstallable() const { return location_ == INTERNAL; }

 private:
  // Helper method that loads a UserScript object from a
  // dictionary in the content_script list of the manifest.
  bool LoadUserScriptHelper(const DictionaryValue* content_script,
                            int definition_index,
                            std::string* error,
                            UserScript* result);

  // Helper method that loads a PageAction object from a
  // dictionary in the page_action list of the manifest.
  PageAction* LoadPageActionHelper(const DictionaryValue* page_action,
                                   int definition_index,
                                   std::string* error);

  // The absolute path to the directory the extension is stored in.
  FilePath path_;

  // The base extension url for the extension.
  GURL extension_url_;

  // The location the extension was loaded from.
  Location location_;

  // A human-readable ID for the extension. The convention is to use something
  // like 'com.example.myextension', but this is not currently enforced. An
  // extension's ID is used in things like directory structures and URLs, and
  // is expected to not change across versions. In the case of conflicts,
  // updates will only be allowed if the extension can be validated using the
  // previous version's update key.
  std::string id_;

  // The extension's version.
  scoped_ptr<Version> version_;

  // The extension's human-readable name.
  std::string name_;

  // An optional longer description of the extension.
  std::string description_;

  // Paths to the content scripts the extension contains.
  UserScriptList content_scripts_;

  // A list of page actions.
  PageActionMap page_actions_;

  // Optional absolute path to the directory of NPAPI plugins that the extension
  // contains.
  FilePath plugins_dir_;

  // Optional URL to a master page of which a single instance should be always
  // loaded in the background.
  GURL background_url_;

  // Paths to HTML files to be displayed in the toolbar.
  std::vector<std::string> toolstrips_;

  // A SHA1 hash of the contents of the zip file.  Note that this key is only
  // present in the manifest that's prepended to the zip.  The inner manifest
  // will not have this key.
  std::string zip_hash_;

  // A map of resource id's to relative file paths.
  scoped_ptr<DictionaryValue> theme_images_;

  // A map of color names to colors.
  scoped_ptr<DictionaryValue> theme_colors_;

  // A map of color names to colors.
  scoped_ptr<DictionaryValue> theme_tints_;

  // Whether the extension is a theme - if it is, certain things are disabled.
  bool is_theme_;

  // The sites this extension has permission to talk to (using XHR, etc).
  std::vector<URLPattern> permissions_;

  // We implement copy, but not assign.
  void operator=(const Extension&);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_H_
