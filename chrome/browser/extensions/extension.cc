// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_util.h"

const char kExtensionURLScheme[] = "chrome-extension";
const char kUserScriptURLScheme[] = "chrome-user-script";

const char Extension::kManifestFilename[] = "manifest";

const wchar_t* Extension::kDescriptionKey = L"description";
const wchar_t* Extension::kFilesKey = L"files";
const wchar_t* Extension::kFormatVersionKey = L"format_version";
const wchar_t* Extension::kIdKey = L"id";
const wchar_t* Extension::kMatchesKey = L"matches";
const wchar_t* Extension::kNameKey = L"name";
const wchar_t* Extension::kUserScriptsKey = L"user_scripts";
const wchar_t* Extension::kVersionKey = L"version";

// Extension-related error messages. Some of these are simple patterns, where a
// '*' is replaced at runtime with a specific value. This is used instead of
// printf because we want to unit test them and scanf is hard to make
// cross-platform.
const char* Extension::kInvalidDescriptionError =
    "Invalid value for 'description'.";
const char* Extension::kInvalidFileCountError =
    "Invalid value for 'user_scripts[*].files. Only one file is currently "
    "supported per-user script.";
const char* Extension::kInvalidFileError =
    "Invalid value for 'user_scripts[*].files[*]'.";
const char* Extension::kInvalidFilesError =
    "Required value 'user_scripts[*].files is missing or invalid.";
const char* Extension::kInvalidFormatVersionError =
    "Required value 'format_version' is missing or invalid.";
const char* Extension::kInvalidIdError =
    "Required value 'id' is missing or invalid.";
const char* Extension::kInvalidManifestError =
    "Manifest is missing or invalid.";
const char* Extension::kInvalidMatchCountError =
    "Invalid value for 'user_scripts[*].matches. There must be at least one "
    "match specified.";
const char* Extension::kInvalidMatchError =
    "Invalid value for 'user_scripts[*].matches[*]'.";
const char* Extension::kInvalidMatchesError =
    "Required value 'user_scripts[*].matches' is missing or invalid.";
const char* Extension::kInvalidNameError =
    "Required value 'name' is missing or invalid.";
const char* Extension::kInvalidUserScriptError =
    "Invalid value for 'user_scripts[*]'.";
const char* Extension::kInvalidUserScriptsListError =
    "Invalid value for 'user_scripts'.";
const char* Extension::kInvalidVersionError =
    "Required value 'version' is missing or invalid.";

// Defined in extension_protocols.h.
extern const char kExtensionURLScheme[];

// static
GURL Extension::GetResourceURL(const GURL& extension_url,
                               const std::string& relative_path) {
  DCHECK(extension_url.SchemeIs(kExtensionURLScheme));
  DCHECK(extension_url.path() == "/");

  GURL ret_val = GURL(extension_url.spec() + relative_path);
  DCHECK(StartsWithASCII(ret_val.spec(), extension_url.spec(), false));

  return ret_val;
}

// static
FilePath Extension::GetResourcePath(const FilePath& extension_path,
                                    const std::string& relative_path) {
  // Build up a file:// URL and convert that back to a FilePath.  This avoids
  // URL encoding and path separator issues.

  // Convert the extension's root to a file:// URL.
  GURL extension_url = net::FilePathToFileURL(extension_path);
  if (!extension_url.is_valid())
    return FilePath();

  // Append the requested path.
  GURL::Replacements replacements;
  std::string new_path(extension_url.path());
  new_path += "/";
  new_path += relative_path;
  replacements.SetPathStr(new_path);
  GURL file_url = extension_url.ReplaceComponents(replacements);
  if (!file_url.is_valid())
    return FilePath();

  // Convert the result back to a FilePath.
  FilePath ret_val;
  if (!net::FileURLToFilePath(file_url, &ret_val))
    return FilePath();

  // Double-check that the path we ended up with is actually inside the
  // extension root. We can do this with a simple prefix match because:
  // a) We control the prefix on both sides, and they should match.
  // b) GURL normalizes things like "../" and "//" before it gets to us.
  if (ret_val.value().find(extension_path.value() +
                           FilePath::kSeparators[0]) != 0)
    return FilePath();

  return ret_val;
}

// Creates an error messages from a pattern.
static std::string FormatErrorMessage(const std::string& format,
                                      const std::string s1) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  return ret_val;
}

static std::string FormatErrorMessage(const std::string& format,
                                      const std::string s1,
                                      const std::string s2) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  return ret_val;
}

Extension::Extension(const FilePath& path) {
  DCHECK(path.IsAbsolute());

#if defined(OS_WIN)
  // Normalize any drive letter to upper-case. We do this for consistency with
  // net_utils::FilePathToFileURL(), which does the same thing, to make string
  // comparisons simpler.
  std::wstring path_str = path.value();
  if (path_str.size() >= 2 && path_str[0] >= L'a' && path_str[0] <= L'z' &&
      path_str[1] == ':')
    path_str[0] += ('A' - 'a');

  path_ = FilePath(path_str);
#else
  path_ = path;
#endif
}

bool Extension::InitFromValue(const DictionaryValue& source,
                              std::string* error) {
  // Check format version.
  int format_version = 0;
  if (!source.GetInteger(kFormatVersionKey, &format_version) ||
      format_version != kExpectedFormatVersion) {
    *error = kInvalidFormatVersionError;
    return false;
  }

  // Initialize id.
  if (!source.GetString(kIdKey, &id_)) {
    *error = kInvalidIdError;
    return false;
  }

  // Initialize URL.
  extension_url_ = GURL(std::string(kExtensionURLScheme) + "://" + id_ + "/");

  // Initialize version.
  if (!source.GetString(kVersionKey, &version_)) {
    *error = kInvalidVersionError;
    return false;
  }

  // Initialize name.
  if (!source.GetString(kNameKey, &name_)) {
    *error = kInvalidNameError;
    return false;
  }

  // Initialize description (optional).
  if (source.HasKey(kDescriptionKey)) {
    if (!source.GetString(kDescriptionKey, &description_)) {
      *error = kInvalidDescriptionError;
      return false;
    }
  }

  // Initialize user scripts (optional).
  if (source.HasKey(kUserScriptsKey)) {
    ListValue* list_value;
    if (!source.GetList(kUserScriptsKey, &list_value)) {
      *error = kInvalidUserScriptsListError;
      return false;
    }

    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      DictionaryValue* user_script;
      if (!list_value->GetDictionary(i, &user_script)) {
        *error = FormatErrorMessage(kInvalidUserScriptError, IntToString(i));
        return false;
      }

      ListValue* matches;
      ListValue* files;

      if (!user_script->GetList(kMatchesKey, &matches)) {
        *error = FormatErrorMessage(kInvalidMatchesError, IntToString(i));
        return false;
      }

      if (!user_script->GetList(kFilesKey, &files)) {
        *error = FormatErrorMessage(kInvalidFilesError, IntToString(i));
        return false;
      }

      if (matches->GetSize() == 0) {
        *error = FormatErrorMessage(kInvalidMatchCountError, IntToString(i));
        return false;
      }

      // NOTE: Only one file is supported right now.
      if (files->GetSize() != 1) {
        *error = FormatErrorMessage(kInvalidFileCountError, IntToString(i));
        return false;
      }

      UserScriptInfo script_info;
      for (size_t j = 0; j < matches->GetSize(); ++j) {
        std::string match;
        if (!matches->GetString(j, &match)) {
          *error = FormatErrorMessage(kInvalidMatchError, IntToString(i),
                                      IntToString(j));
          return false;
        }

        script_info.matches.push_back(match);
      }

      // TODO(aa): Support multiple files.
      std::string file;
      if (!files->GetString(0, &file)) {
        *error = FormatErrorMessage(kInvalidFileError, IntToString(i),
                                    IntToString(0));
        return false;
      }
      script_info.path = Extension::GetResourcePath(path(), file);
      script_info.url = Extension::GetResourceURL(url(), file);

      user_scripts_.push_back(script_info);
    }
  }

  return true;
}
