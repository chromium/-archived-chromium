// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/string16.h"
#include "base/values.h"

// Represents a Chromium extension.
class Extension {
 public:
  Extension(){};
  Extension(const FilePath& path) : path_(path) {};

  // The format for extension manifests that this code understands.
  static const int kExpectedFormatVersion = 1;

  // The name of the manifest inside an extension.
  static const FilePath::CharType* kManifestFilename;

  // Keys used in JSON representation of extensions.
  static const wchar_t* kFormatVersionKey;
  static const wchar_t* kIdKey;
  static const wchar_t* kNameKey;
  static const wchar_t* kDescriptionKey;
  static const wchar_t* kContentScriptsKey;

  // Error messages returned from InitFromValue().
  static const char* kInvalidFormatVersionError;
  static const char* kInvalidManifestError;
  static const char* kInvalidIdError;
  static const char* kInvalidNameError;
  static const char* kInvalidDescriptionError;
  static const char* kInvalidContentScriptsListError;
  static const char* kInvalidContentScriptError;

  // The path to the folder the extension is stored in.
  const FilePath& path() const { return path_; }

  // A human-readable ID for the extension. The convention is to use something
  // like 'com.example.myextension', but this is not currently enforced. An
  // extension's ID is used in things like directory structures and URLs, and
  // is expected to not change across versions. In the case of conflicts,
  // updates will only be allowed if the extension can be validated using the
  // previous version's update key.
  const std::string& id() const { return id_; }

  // A human-readable name of the extension.
  const std::string& name() const { return name_; }

  // An optional longer description of the extension.
  const std::string& description() const { return description_; }

  // Paths to the content scripts that the extension contains.
  const std::vector<std::string>& content_scripts() const {
    return content_scripts_;
  }

  // Initialize the extension from a parsed manifest.
  bool InitFromValue(const DictionaryValue& value, std::string* error);

  // Serialize the extension to a DictionaryValue.
  void CopyToValue(DictionaryValue* value);

 private:
  // The path to the directory the extension is stored in.
  FilePath path_;

  // The extension's ID.
  std::string id_;

  // The extension's human-readable name.
  std::string name_;

  // An optional description for the extension.
  std::string description_;

  // Paths to the content scripts the extension contains.
  std::vector<std::string> content_scripts_;

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_H_
