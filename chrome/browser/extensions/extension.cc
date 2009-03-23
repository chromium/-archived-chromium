// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/url_constants.h"

const char Extension::kManifestFilename[] = "manifest.json";

const wchar_t* Extension::kContentScriptsKey = L"content_scripts";
const wchar_t* Extension::kCssKey = L"css";
const wchar_t* Extension::kDescriptionKey = L"description";
const wchar_t* Extension::kFormatVersionKey = L"format_version";
const wchar_t* Extension::kIdKey = L"id";
const wchar_t* Extension::kJsKey = L"js";
const wchar_t* Extension::kMatchesKey = L"matches";
const wchar_t* Extension::kNameKey = L"name";
const wchar_t* Extension::kPermissionsKey = L"permissions";
const wchar_t* Extension::kPluginsDirKey = L"plugins_dir";
const wchar_t* Extension::kRunAtKey = L"run_at";
const wchar_t* Extension::kThemeKey = L"theme";
const wchar_t* Extension::kToolstripsKey = L"toolstrips";
const wchar_t* Extension::kVersionKey = L"version";
const wchar_t* Extension::kZipHashKey = L"zip_hash";

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
const char* Extension::kInvalidCssError =
    "Invalid value for 'content_scripts[*].css[*]'.";
const char* Extension::kInvalidCssListError =
    "Required value 'content_scripts[*].css is invalid.";
const char* Extension::kInvalidDescriptionError =
    "Invalid value for 'description'.";
const char* Extension::kInvalidFormatVersionError =
    "Required value 'format_version' is missing or invalid.";
const char* Extension::kInvalidIdError =
    "Required value 'id' is missing or invalid.";
const char* Extension::kInvalidJsError =
    "Invalid value for 'content_scripts[*].js[*]'.";
const char* Extension::kInvalidJsListError =
    "Required value 'content_scripts[*].js is invalid.";
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
const char* Extension::kInvalidPermissionsError =
    "Required value 'permissions' is missing or invalid.";
const char* Extension::kInvalidPermissionCountWarning =
    "Warning, 'permissions' key found, but array is empty.";
const char* Extension::kInvalidPermissionError =
    "Invalid value for 'permissions[*]'.";
const char* Extension::kInvalidPermissionSchemeError =
    "Invalid scheme for 'permissions[*]'. Only 'http' and 'https' are "
    "allowed.";
const char* Extension::kInvalidPluginsDirError =
    "Invalid value for 'plugins_dir'.";
const char* Extension::kInvalidRunAtError =
    "Invalid value for 'content_scripts[*].run_at'.";
const char* Extension::kInvalidToolstripError =
    "Invalid value for 'toolstrips[*]'";
const char* Extension::kInvalidToolstripsError =
    "Invalid value for 'toolstrips'.";
const char* Extension::kInvalidVersionError =
    "Required value 'version' is missing or invalid.";
const char* Extension::kInvalidZipHashError =
    "Required key 'zip_hash' is missing or invalid.";
const char* Extension::kMissingFileError =
    "At least one js or css file is required for 'content_scripts[*]'.";

const size_t Extension::kIdSize = 20;  // SHA1 (160 bits) == 20 bytes

Extension::Extension(const Extension& rhs)
    : path_(rhs.path_),
      extension_url_(rhs.extension_url_),
      id_(rhs.id_),
      version_(new Version(*rhs.version_)),
      name_(rhs.name_),
      description_(rhs.description_),
      content_scripts_(rhs.content_scripts_),
      plugins_dir_(rhs.plugins_dir_),
      zip_hash_(rhs.zip_hash_),
      theme_paths_(rhs.theme_paths_) {
}

const std::string Extension::VersionString() const {
  return version_->GetString();
}

// static
GURL Extension::GetResourceURL(const GURL& extension_url,
                               const std::string& relative_path) {
  DCHECK(extension_url.SchemeIs(chrome::kExtensionScheme));
  DCHECK(extension_url.path() == "/");

  GURL ret_val = GURL(extension_url.spec() + relative_path);
  DCHECK(StartsWithASCII(ret_val.spec(), extension_url.spec(), false));

  return ret_val;
}

FilePath Extension::GetThemeResourcePath(const int resource_id) {
  std::wstring id = IntToWString(resource_id);
  std::string path = theme_paths_[id];
  if (path.size())
    return path_.AppendASCII(path.c_str());
  return FilePath();
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

// Helper method that loads a UserScript object from a dictionary in the
// content_script list of the manifest.
bool Extension::LoadUserScriptHelper(const DictionaryValue* content_script,
                                     int definition_index, std::string* error,
                                     UserScript* result) {
  // run_at
  if (content_script->HasKey(kRunAtKey)) {
    std::string run_location;
    if (!content_script->GetString(kRunAtKey, &run_location)) {
      *error = FormatErrorMessage(kInvalidRunAtError,
                                  IntToString(definition_index));
      return false;
    }

    if (run_location == kRunAtDocumentStartValue) {
      result->set_run_location(UserScript::DOCUMENT_START);
    } else if (run_location == kRunAtDocumentEndValue) {
      result->set_run_location(UserScript::DOCUMENT_END);
    } else {
      *error = FormatErrorMessage(kInvalidRunAtError,
                                  IntToString(definition_index));
      return false;
    }
  }

  // matches
  ListValue* matches = NULL;
  if (!content_script->GetList(kMatchesKey, &matches)) {
    *error = FormatErrorMessage(kInvalidMatchesError,
                                IntToString(definition_index));
    return false;
  }

  if (matches->GetSize() == 0) {
    *error = FormatErrorMessage(kInvalidMatchCountError,
                                IntToString(definition_index));
    return false;
  }
  for (size_t j = 0; j < matches->GetSize(); ++j) {
    std::string match_str;
    if (!matches->GetString(j, &match_str)) {
      *error = FormatErrorMessage(kInvalidMatchError,
                                  IntToString(definition_index),
                                  IntToString(j));
      return false;
    }

    URLPattern pattern;
    if (!pattern.Parse(match_str)) {
      *error = FormatErrorMessage(kInvalidMatchError,
                                  IntToString(definition_index),
                                  IntToString(j));
      return false;
    }

    result->add_url_pattern(pattern);
  }

  // js and css keys
  ListValue* js = NULL;
  if (content_script->HasKey(kJsKey) &&
      !content_script->GetList(kJsKey, &js)) {
    *error = FormatErrorMessage(kInvalidJsListError,
                                IntToString(definition_index));
    return false;
  }

  ListValue* css = NULL;
  if (content_script->HasKey(kCssKey) &&
      !content_script->GetList(kCssKey, &css)) {
    *error = FormatErrorMessage(kInvalidCssListError,
      IntToString(definition_index));
    return false;
  }

  // The manifest needs to have at least one js or css user script definition.
  if (((js ? js->GetSize() : 0) + (css ? css->GetSize() : 0)) == 0) {
    *error = FormatErrorMessage(kMissingFileError,
                                IntToString(definition_index));
    return false;
  }

  if (js) {
    for (size_t script_index = 0; script_index < js->GetSize();
         ++script_index) {
      Value* value;
      std::wstring relative;
      if (!js->Get(script_index, &value) || !value->GetAsString(&relative)) {
        *error = FormatErrorMessage(kInvalidJsError,
                                    IntToString(definition_index),
                                    IntToString(script_index));
        return false;
      }
      // TODO(georged): Make GetResourceURL accept wstring too
      GURL url = GetResourceURL(WideToUTF8(relative));
      FilePath path = GetResourcePath(WideToUTF8(relative));
      result->js_scripts().push_back(UserScript::File(path, url));
    }
  }

  if (css) {
    for (size_t script_index = 0; script_index < css->GetSize();
         ++script_index) {
      Value* value;
      std::wstring relative;
      if (!css->Get(script_index, &value) || !value->GetAsString(&relative)) {
        *error = FormatErrorMessage(kInvalidCssError,
                                    IntToString(definition_index),
                                    IntToString(script_index));
        return false;
      }
      // TODO(georged): Make GetResourceURL accept wstring too
      GURL url = GetResourceURL(WideToUTF8(relative));
      FilePath path = GetResourcePath(WideToUTF8(relative));
      result->css_scripts().push_back(UserScript::File(path, url));
    }
  }

  return true;
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

  // Normalize the string to lowercase, so it can be used as an URL component
  // (where GURL will lowercase it).
  StringToLowerASCII(&id_);

  // Verify that the id is legal.  The id is a hex string of the SHA-1 hash of
  // the public key.
  std::vector<uint8> id_bytes;
  if (!HexStringToBytes(id_, &id_bytes) || id_bytes.size() != kIdSize) {
    *error = kInvalidIdError;
    return false;
  }

  // Initialize URL.
  extension_url_ = GURL(std::string(chrome::kExtensionScheme) +
                        chrome::kStandardSchemeSeparator + id_ + "/");

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

  // Initialize toolstrips (optional).
  if (source.HasKey(kToolstripsKey)) {
    ListValue* list_value;
    if (!source.GetList(kToolstripsKey, &list_value)) {
      *error = kInvalidToolstripsError;
      return false;
    }

    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      std::string toolstrip;
      if (!list_value->GetString(i, &toolstrip)) {
        *error = FormatErrorMessage(kInvalidToolstripError, IntToString(i));
        return false;
      }
      toolstrips_.push_back(toolstrip);
    }
  }

  if (source.HasKey(kThemeKey)) {
    DictionaryValue* dict_value;
    if (source.GetDictionary(kThemeKey, &dict_value)) {
      DictionaryValue::key_iterator iter = dict_value->begin_keys();
      while (iter != dict_value->end_keys()) {
        std::string val;
        if (dict_value->GetString(*iter, &val)) {
          std::wstring id = *iter;
          theme_paths_[id] = val;
        }
        ++iter;
      }
      ResourceBundle::GetSharedInstance().SetThemeExtension(*this);
    }
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
        *error = FormatErrorMessage(kInvalidContentScriptError,
          IntToString(i));
        return false;
      }

      UserScript script;
      if (!LoadUserScriptHelper(content_script, i, error, &script))
        return false;  // Failed to parse script context definition
      content_scripts_.push_back(script);
    }
  }

  // Initialize the permissions (optional).
  if (source.HasKey(kPermissionsKey)) {
    ListValue* hosts = NULL;
    if (!source.GetList(kPermissionsKey, &hosts)) {
      *error = FormatErrorMessage(kInvalidPermissionsError, "");
      return false;
    }

    if (hosts->GetSize() == 0) {
      ExtensionErrorReporter::GetInstance()->ReportError(
          kInvalidPermissionCountWarning, false);
    }

    for (size_t i = 0; i < hosts->GetSize(); ++i) {
      std::string host_str;
      if (!hosts->GetString(i, &host_str)) {
        *error = FormatErrorMessage(kInvalidPermissionError,
                                    IntToString(i));
        return false;
      }

      URLPattern pattern;
      if (!pattern.Parse(host_str)) {
        *error = FormatErrorMessage(kInvalidPermissionError,
                                    IntToString(i));
        return false;
      }

      // Only accept http/https persmissions at the moment.
      if ((pattern.scheme() != chrome::kHttpScheme) &&
          (pattern.scheme() != chrome::kHttpsScheme)) {
        *error = FormatErrorMessage(kInvalidPermissionSchemeError,
                                    IntToString(i));
        return false;
      }

      permissions_.push_back(pattern);
    }
  }

  return true;
}
