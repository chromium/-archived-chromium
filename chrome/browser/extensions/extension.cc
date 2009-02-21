// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "chrome/common/extensions/user_script.h"

const char kExtensionURLScheme[] = "chrome-extension";
const char kUserScriptURLScheme[] = "chrome-user-script";

const char Extension::kManifestFilename[] = "manifest.json";

const wchar_t* Extension::kContentScriptsKey = L"content_scripts";
const wchar_t* Extension::kDescriptionKey = L"description";
const wchar_t* Extension::kFormatVersionKey = L"format_version";
const wchar_t* Extension::kIdKey = L"id";
const wchar_t* Extension::kJsKey = L"js";
const wchar_t* Extension::kMatchesKey = L"matches";
const wchar_t* Extension::kNameKey = L"name";
const wchar_t* Extension::kRunAtKey = L"run_at";
const wchar_t* Extension::kVersionKey = L"version";
const wchar_t* Extension::kZipHashKey = L"zip_hash";
const wchar_t* Extension::kPluginsDirKey = L"plugins_dir";

const char* Extension::kRunAtDocumentStartValue = "document_start";
const char* Extension::kRunAtDocumentEndValue = "document_end";

// Extension-related error messages. Some of these are simple patterns, where a
// '*' is replaced at runtime with a specific value. This is used instead of
// printf because we want to unit test them and scanf is hard to make
// cross-platform.
const char* Extension::kInvalidContentScriptError =
    "Invalid value for 'content_scripts[*]'.";
const char* Extension::kInvalidContentScriptsListError =
    "Invalid value for 'content_scripts'.";
const char* Extension::kInvalidDescriptionError =
    "Invalid value for 'description'.";
const char* Extension::kInvalidFormatVersionError =
    "Required value 'format_version' is missing or invalid.";
const char* Extension::kInvalidIdError =
    "Required value 'id' is missing or invalid.";
const char* Extension::kInvalidJsCountError =
    "Invalid value for 'content_scripts[*].js. Only one js file is currently "
    "supported per-content script.";
const char* Extension::kInvalidJsError =
    "Invalid value for 'content_scripts[*].js[*]'.";
const char* Extension::kInvalidJsListError =
    "Required value 'content_scripts[*].js is missing or invalid.";
const char* Extension::kInvalidManifestError =
    "Manifest is missing or invalid.";
const char* Extension::kInvalidMatchCountError =
    "Invalid value for 'content_scripts[*].matches. There must be at least one "
    "match specified.";
const char* Extension::kInvalidMatchError =
    "Invalid value for 'content_scripts[*].matches[*]'.";
const char* Extension::kInvalidMatchesError =
    "Required value 'content_scripts[*].matches' is missing or invalid.";
const char* Extension::kInvalidNameError =
    "Required value 'name' is missing or invalid.";
const char* Extension::kInvalidRunAtError =
    "Invalid value for 'content_scripts[*].run_at'.";
const char* Extension::kInvalidVersionError =
    "Required value 'version' is missing or invalid.";
const char* Extension::kInvalidZipHashError =
    "Required key 'zip_hash' is missing or invalid.";
const char* Extension::kInvalidPluginsDirError =
    "Invalid value for 'plugins_dir'.";

const std::string Extension::VersionString() const {
  return version_->GetString();
}

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
      static_cast<uint32>(format_version) != kExpectedFormatVersion) {
    *error = kInvalidFormatVersionError;
    return false;
  }

  // Initialize id.
  if (!source.GetString(kIdKey, &id_)) {
    *error = kInvalidIdError;
    return false;
  }
  // Verify that the id is legal.  This test is basically verifying that it
  // is ASCII and doesn't have any path components in it.
  // TODO(erikkay): verify the actual id format - it will be more restrictive
  // than this.  Perhaps just a hex string?
  if (!IsStringASCII(id_)) {
    *error = kInvalidIdError;
    return false;
  }
  FilePath id_path;
  id_path = id_path.AppendASCII(id_);
  if ((id_path.value() == FilePath::kCurrentDirectory) ||
      (id_path.value() == FilePath::kParentDirectory) ||
      !(id_path.BaseName() == id_path)) {
    *error = kInvalidIdError;
    return false;
  }

  // Initialize URL.
  extension_url_ = GURL(std::string(kExtensionURLScheme) + "://" + id_ + "/");

  // Initialize version.
  std::string version_str;
  if (!source.GetString(kVersionKey, &version_str)) {
    *error = kInvalidVersionError;
    return false;
  }
  version_.reset(Version::GetVersionFromString(version_str));
  if (!version_.get()) {
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

  // Initialize zip hash (only present in zip)
  // There's no need to verify it at this point. If it's in a bogus format
  // it won't pass the hash verify step.
  if (source.HasKey(kZipHashKey)) {
    if (!source.GetString(kZipHashKey, &zip_hash_)) {
      *error = kInvalidZipHashError;
      return false;
    }
  }

  // Initialize plugins dir (optional).
  if (source.HasKey(kPluginsDirKey)) {
    std::string plugins_dir;
    if (!source.GetString(kPluginsDirKey, &plugins_dir)) {
      *error = kInvalidPluginsDirError;
      return false;
    }
    plugins_dir_ = path_.AppendASCII(plugins_dir);
  }

  // Initialize content scripts (optional).
  if (source.HasKey(kContentScriptsKey)) {
    ListValue* list_value;
    if (!source.GetList(kContentScriptsKey, &list_value)) {
      *error = kInvalidContentScriptsListError;
      return false;
    }

    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      DictionaryValue* content_script;
      if (!list_value->GetDictionary(i, &content_script)) {
        *error = FormatErrorMessage(kInvalidContentScriptError, IntToString(i));
        return false;
      }

      ListValue* matches;
      ListValue* js;

      if (!content_script->GetList(kMatchesKey, &matches)) {
        *error = FormatErrorMessage(kInvalidMatchesError, IntToString(i));
        return false;
      }

      if (!content_script->GetList(kJsKey, &js)) {
        *error = FormatErrorMessage(kInvalidJsListError, IntToString(i));
        return false;
      }

      if (matches->GetSize() == 0) {
        *error = FormatErrorMessage(kInvalidMatchCountError, IntToString(i));
        return false;
      }

      // NOTE: Only one file is supported right now.
      if (js->GetSize() != 1) {
        *error = FormatErrorMessage(kInvalidJsCountError, IntToString(i));
        return false;
      }

      UserScript script;
      if (content_script->HasKey(kRunAtKey)) {
        std::string run_location;
        if (!content_script->GetString(kRunAtKey, &run_location)) {
          *error = FormatErrorMessage(kInvalidRunAtError, IntToString(i));
          return false;
        }

        if (run_location == kRunAtDocumentStartValue) {
          script.set_run_location(UserScript::DOCUMENT_START);
        } else if (run_location == kRunAtDocumentEndValue) {
          script.set_run_location(UserScript::DOCUMENT_END);
        } else {
          *error = FormatErrorMessage(kInvalidRunAtError, IntToString(i));
          return false;
        }
      }

      for (size_t j = 0; j < matches->GetSize(); ++j) {
        std::string match_str;
        if (!matches->GetString(j, &match_str)) {
          *error = FormatErrorMessage(kInvalidMatchError, IntToString(i),
                                      IntToString(j));
          return false;
        }

        URLPattern pattern;
        if (!pattern.Parse(match_str)) {
          *error = FormatErrorMessage(kInvalidMatchError, IntToString(i),
                                      IntToString(j));
          return false;
        }

        script.add_url_pattern(pattern);
      }

      // TODO(aa): Support multiple files.
      std::string file;
      if (!js->GetString(0, &file)) {
        *error = FormatErrorMessage(kInvalidJsError, IntToString(i),
                                    IntToString(0));
        return false;
      }
      script.set_path(Extension::GetResourcePath(path(), file));
      script.set_url(Extension::GetResourceURL(url(), file));

      content_scripts_.push_back(script);
    }
  }

  return true;
}

