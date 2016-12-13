// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/url_constants.h"
#include "net/base/base64.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "base/registry.h"
#endif

namespace {
  const int kPEMOutputColumns = 65;

  // KEY MARKERS
  const char kKeyBeginHeaderMarker[] = "-----BEGIN";
  const char kKeyBeginFooterMarker[] = "-----END";
  const char kKeyInfoEndMarker[] = "KEY-----";
  const char kPublic[] = "PUBLIC";
  const char kPrivate[] = "PRIVATE";

  const int kRSAKeySize = 1024;

  // Converts a normal hexadecimal string into the alphabet used by extensions.
  // We use the characters 'a'-'p' instead of '0'-'f' to avoid ever having a
  // completely numeric host, since some software interprets that as an IP
  // address.
  static void ConvertHexadecimalToIDAlphabet(std::string* id) {
    for (size_t i = 0; i < id->size(); ++i)
      (*id)[i] = HexStringToInt(id->substr(i, 1)) + 'a';
  }
};

int Extension::id_counter_ = 0;

const char Extension::kManifestFilename[] = "manifest.json";

const wchar_t* Extension::kBackgroundKey = L"background_page";
const wchar_t* Extension::kContentScriptsKey = L"content_scripts";
const wchar_t* Extension::kCssKey = L"css";
const wchar_t* Extension::kDescriptionKey = L"description";
const wchar_t* Extension::kIconPathKey = L"icon";
const wchar_t* Extension::kIconPathsKey = L"icons";
const wchar_t* Extension::kJsKey = L"js";
const wchar_t* Extension::kMatchesKey = L"matches";
const wchar_t* Extension::kNameKey = L"name";
const wchar_t* Extension::kPageActionIdKey = L"id";
const wchar_t* Extension::kPageActionsKey = L"page_actions";
const wchar_t* Extension::kPermissionsKey = L"permissions";
const wchar_t* Extension::kPluginsKey = L"plugins";
const wchar_t* Extension::kPluginsPathKey = L"path";
const wchar_t* Extension::kPluginsPublicKey = L"public";
const wchar_t* Extension::kPublicKeyKey = L"key";
const wchar_t* Extension::kRunAtKey = L"run_at";
const wchar_t* Extension::kSignatureKey = L"signature";
const wchar_t* Extension::kThemeKey = L"theme";
const wchar_t* Extension::kThemeImagesKey = L"images";
const wchar_t* Extension::kThemeColorsKey = L"colors";
const wchar_t* Extension::kThemeTintsKey = L"tints";
const wchar_t* Extension::kThemeDisplayPropertiesKey = L"properties";
const wchar_t* Extension::kToolstripsKey = L"toolstrips";
const wchar_t* Extension::kTypeKey = L"type";
const wchar_t* Extension::kVersionKey = L"version";
const wchar_t* Extension::kUpdateURLKey = L"update_url";

const char* Extension::kRunAtDocumentStartValue = "document_start";
const char* Extension::kRunAtDocumentEndValue = "document_end";
const char* Extension::kPageActionTypeTab = "tab";
const char* Extension::kPageActionTypePermanent = "permanent";

// A list of all the keys allowed by themes.
static const wchar_t* kValidThemeKeys[] = {
  Extension::kDescriptionKey,
  Extension::kIconPathKey,
  Extension::kNameKey,
  Extension::kPublicKeyKey,
  Extension::kSignatureKey,
  Extension::kThemeKey,
  Extension::kVersionKey
};

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
const char* Extension::kInvalidJsError =
    "Invalid value for 'content_scripts[*].js[*]'.";
const char* Extension::kInvalidJsListError =
    "Required value 'content_scripts[*].js is invalid.";
const char* Extension::kInvalidKeyError =
    "Value 'key' is missing or invalid.";
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
const char* Extension::kInvalidPageActionError =
    "Invalid value for 'page_actions[*]'.";
const char* Extension::kInvalidPageActionIconPathError =
    "Invalid value for 'page_actions[*].icons[*]'.";
const char* Extension::kInvalidPageActionsListError =
    "Invalid value for 'page_actions'.";
const char* Extension::kInvalidPageActionIconPathsError =
    "Required value 'page_actions[*].icons' is missing or invalid.";
const char* Extension::kInvalidPageActionIdError =
    "Required value 'id' is missing or invalid.";
const char* Extension::kInvalidPageActionTypeValueError =
    "Invalid value for 'page_actions[*].type', expected 'tab' or 'permanent'.";
const char* Extension::kInvalidPermissionsError =
    "Required value 'permissions' is missing or invalid.";
const char* Extension::kInvalidPermissionCountWarning =
    "Warning, 'permissions' key found, but array is empty.";
const char* Extension::kInvalidPermissionError =
    "Invalid value for 'permissions[*]'.";
const char* Extension::kInvalidPermissionSchemeError =
    "Invalid scheme for 'permissions[*]'. Only 'http' and 'https' are "
    "allowed.";
const char* Extension::kInvalidPluginsError =
    "Invalid value for 'plugins'.";
const char* Extension::kInvalidPluginsPathError =
    "Invalid value for 'plugins[*].path'.";
const char* Extension::kInvalidPluginsPublicError =
    "Invalid value for 'plugins[*].public'.";
const char* Extension::kInvalidBackgroundError =
    "Invalid value for 'background'.";
const char* Extension::kInvalidRunAtError =
    "Invalid value for 'content_scripts[*].run_at'.";
const char* Extension::kInvalidSignatureError =
    "Value 'signature' is missing or invalid.";
const char* Extension::kInvalidToolstripError =
    "Invalid value for 'toolstrips[*]'";
const char* Extension::kInvalidToolstripsError =
    "Invalid value for 'toolstrips'.";
const char* Extension::kInvalidVersionError =
    "Required value 'version' is missing or invalid. It must be between 1-4 "
    "dot-separated integers.";
const char* Extension::kInvalidZipHashError =
    "Required key 'zip_hash' is missing or invalid.";
const char* Extension::kMissingFileError =
    "At least one js or css file is required for 'content_scripts[*]'.";
const char* Extension::kInvalidThemeError =
    "Invalid value for 'theme'.";
const char* Extension::kInvalidThemeImagesError =
    "Invalid value for theme images - images must be strings.";
const char* Extension::kInvalidThemeImagesMissingError =
    "Am image specified in the theme is missing.";
const char* Extension::kInvalidThemeColorsError =
    "Invalid value for theme colors - colors must be integers";
const char* Extension::kInvalidThemeTintsError =
    "Invalid value for theme images - tints must be decimal numbers.";
const char* Extension::kInvalidUpdateURLError =
    "Invalid value for update url: '[*]'.";
const char* Extension::kThemesCannotContainExtensionsError =
    "A theme cannot contain extensions code.";

#if defined(OS_WIN)
const char* Extension::kExtensionRegistryPath =
    "Software\\Google\\Chrome\\Extensions";
#endif

// first 16 bytes of SHA256 hashed public key.
const size_t Extension::kIdSize = 16;

Extension::~Extension() {
  for (PageActionMap::iterator i = page_actions_.begin();
       i != page_actions_.end(); ++i)
    delete i->second;
}

const std::string Extension::VersionString() const {
  return version_->GetString();
}

// static
bool Extension::IsExtension(const FilePath& file_name) {
  return file_name.MatchesExtension(
      FilePath::StringType(FILE_PATH_LITERAL(".")) +
      chrome::kExtensionFileExtension);
}

// static
bool Extension::IdIsValid(const std::string& id) {
  // Verify that the id is legal.
  if (id.size() != (kIdSize * 2))
    return false;

  // We only support lowercase IDs, because IDs can be used as URL components
  // (where GURL will lowercase it).
  std::string temp = StringToLowerASCII(id);
  for (size_t i = 0; i < temp.size(); i++)
    if (temp[i] < 'a' || temp[i] > 'p')
      return false;

  return true;
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

const PageAction* Extension::GetPageAction(std::string id) const {
  PageActionMap::const_iterator it = page_actions_.find(id);
  if (it == page_actions_.end())
    return NULL;

  return it->second;
}

Extension::Location Extension::ExternalExtensionInstallType(
    std::string registry_path) {
#if defined(OS_WIN)
  HKEY reg_root = HKEY_LOCAL_MACHINE;
  RegKey key;
  registry_path.append("\\");
  registry_path.append(id_);
  if (key.Open(reg_root, ASCIIToWide(registry_path).c_str()))
    return Extension::EXTERNAL_REGISTRY;
#endif
  return Extension::EXTERNAL_PREF;
}

bool Extension::GenerateIdFromPublicKey(const std::string& input,
                                        std::string* output) {
  CHECK(output);
  if (input.length() == 0)
    return false;

  const uint8* ubuf = reinterpret_cast<const unsigned char*>(input.data());
  SHA256Context ctx;
  SHA256_Begin(&ctx);
  SHA256_Update(&ctx, ubuf, input.length());
  uint8 hash[Extension::kIdSize];
  SHA256_End(&ctx, hash, NULL, sizeof(hash));
  *output = StringToLowerASCII(HexEncode(hash, sizeof(hash)));
  ConvertHexadecimalToIDAlphabet(output);

  return true;
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
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidRunAtError,
          IntToString(definition_index));
      return false;
    }

    if (run_location == kRunAtDocumentStartValue) {
      result->set_run_location(UserScript::DOCUMENT_START);
    } else if (run_location == kRunAtDocumentEndValue) {
      result->set_run_location(UserScript::DOCUMENT_END);
    } else {
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidRunAtError,
          IntToString(definition_index));
      return false;
    }
  }

  // matches
  ListValue* matches = NULL;
  if (!content_script->GetList(kMatchesKey, &matches)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidMatchesError,
        IntToString(definition_index));
    return false;
  }

  if (matches->GetSize() == 0) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidMatchCountError,
        IntToString(definition_index));
    return false;
  }
  for (size_t j = 0; j < matches->GetSize(); ++j) {
    std::string match_str;
    if (!matches->GetString(j, &match_str)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidMatchError,
          IntToString(definition_index), IntToString(j));
      return false;
    }

    URLPattern pattern;
    if (!pattern.Parse(match_str)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidMatchError,
          IntToString(definition_index), IntToString(j));
      return false;
    }

    result->add_url_pattern(pattern);
  }

  // js and css keys
  ListValue* js = NULL;
  if (content_script->HasKey(kJsKey) &&
      !content_script->GetList(kJsKey, &js)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidJsListError,
        IntToString(definition_index));
    return false;
  }

  ListValue* css = NULL;
  if (content_script->HasKey(kCssKey) &&
      !content_script->GetList(kCssKey, &css)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidCssListError,
        IntToString(definition_index));
    return false;
  }

  // The manifest needs to have at least one js or css user script definition.
  if (((js ? js->GetSize() : 0) + (css ? css->GetSize() : 0)) == 0) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kMissingFileError,
        IntToString(definition_index));
    return false;
  }

  if (js) {
    for (size_t script_index = 0; script_index < js->GetSize();
         ++script_index) {
      Value* value;
      std::wstring relative;
      if (!js->Get(script_index, &value) || !value->GetAsString(&relative)) {
        *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidJsError,
            IntToString(definition_index), IntToString(script_index));
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
        *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidCssError,
            IntToString(definition_index), IntToString(script_index));
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

// Helper method that loads a PageAction object from a dictionary in the
// page_action list of the manifest.
PageAction* Extension::LoadPageActionHelper(
    const DictionaryValue* page_action, int definition_index,
    std::string* error) {
  scoped_ptr<PageAction> result(new PageAction());
  result->set_extension_id(id());

  ListValue* icons;
  // Read the page action |icons|.
  if (!page_action->HasKey(kIconPathsKey) ||
      !page_action->GetList(kIconPathsKey, &icons) ||
      icons->GetSize() == 0) {
    *error = ExtensionErrorUtils::FormatErrorMessage(
        kInvalidPageActionIconPathsError, IntToString(definition_index));
    return NULL;
  }

  int icon_count = 0;
  for (ListValue::const_iterator iter = icons->begin();
       iter != icons->end(); ++iter) {
    FilePath::StringType path;
    if (!(*iter)->GetAsString(&path) || path.empty()) {
      *error = ExtensionErrorUtils::FormatErrorMessage(
          kInvalidPageActionIconPathError,
          IntToString(definition_index), IntToString(icon_count));
      return NULL;
    }

    FilePath icon_path = path_.Append(path);
    result->AddIconPath(icon_path);
    ++icon_count;
  }

  // Read the page action |id|.
  std::string id;
  if (!page_action->GetString(kPageActionIdKey, &id)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidPageActionIdError,
        IntToString(definition_index));
    return NULL;
  }
  result->set_id(id);

  // Read the page action |name|.
  std::string name;
  if (!page_action->GetString(kNameKey, &name)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidNameError,
        IntToString(definition_index));
    return NULL;
  }
  result->set_name(name);

  // Read the page action |type|. It is optional and set to permanent if
  // missing.
  std::string type;
  if (!page_action->GetString(kTypeKey, &type)) {
    result->set_type(PageAction::PERMANENT);
  } else if (!LowerCaseEqualsASCII(type, kPageActionTypeTab) &&
             !LowerCaseEqualsASCII(type, kPageActionTypePermanent)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(
        kInvalidPageActionTypeValueError, IntToString(definition_index));
    return NULL;
  } else {
    if (LowerCaseEqualsASCII(type, kPageActionTypeTab))
      result->set_type(PageAction::TAB);
    else
      result->set_type(PageAction::PERMANENT);
  }

  return result.release();
}

bool Extension::ContainsNonThemeKeys(const DictionaryValue& source) {
  // Generate a map of allowable keys
  static std::map<std::wstring, bool> theme_keys;
  static bool theme_key_mapped = false;
  if (!theme_key_mapped) {
    for (size_t i = 0; i < arraysize(kValidThemeKeys); ++i) {
      theme_keys[kValidThemeKeys[i]] = true;
    }
    theme_key_mapped = true;
  }

  // Go through all the root level keys and verify that they're in the map
  // of keys allowable by themes. If they're not, then make a not of it for
  // later.
  DictionaryValue::key_iterator iter = source.begin_keys();
  while (iter != source.end_keys()) {
    std::wstring key = (*iter);
    if (theme_keys.find(key) == theme_keys.end())
      return true;
    ++iter;
  }
  return false;
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
  // extension root.
  if (!extension_path.IsParent(ret_val))
    return FilePath();

  return ret_val;
}

Extension::Extension(const FilePath& path) : is_theme_(false) {
  DCHECK(path.IsAbsolute());
  location_ = INVALID;

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

// TODO(rafaelw): Move ParsePEMKeyBytes, ProducePEM & FormatPEMForOutput to a
// util class in base:
// http://code.google.com/p/chromium/issues/detail?id=13572
bool Extension::ParsePEMKeyBytes(const std::string& input,
                                        std::string* output) {
  DCHECK(output);
  if (!output)
    return false;
  if (input.length() == 0)
    return false;

  std::string working = input;
  if (StartsWithASCII(working, kKeyBeginHeaderMarker, true)) {
    working = CollapseWhitespaceASCII(working, true);
    size_t header_pos = working.find(kKeyInfoEndMarker,
      sizeof(kKeyBeginHeaderMarker) - 1);
    if (header_pos == std::string::npos)
      return false;
    size_t start_pos = header_pos + sizeof(kKeyInfoEndMarker) - 1;
    size_t end_pos = working.rfind(kKeyBeginFooterMarker);
    if (end_pos == std::string::npos)
      return false;
    if (start_pos >= end_pos)
      return false;

    working = working.substr(start_pos, end_pos - start_pos);
    if (working.length() == 0)
      return false;
  }

  return net::Base64Decode(working, output);
}

bool Extension::ProducePEM(const std::string& input,
                                  std::string* output) {
  CHECK(output);
  if (input.length() == 0)
    return false;

  return net::Base64Encode(input, output);
}

bool Extension::FormatPEMForFileOutput(const std::string input,
                                              std::string* output,
                                              bool is_public) {
  CHECK(output);
  if (input.length() == 0)
    return false;
  *output = "";
  output->append(kKeyBeginHeaderMarker);
  output->append(" ");
  output->append(is_public ? kPublic : kPrivate);
  output->append(" ");
  output->append(kKeyInfoEndMarker);
  output->append("\n");
  for (size_t i = 0; i < input.length(); ) {
    int slice = std::min<int>(input.length() - i, kPEMOutputColumns);
    output->append(input.substr(i, slice));
    output->append("\n");
    i += slice;
  }
  output->append(kKeyBeginFooterMarker);
  output->append(" ");
  output->append(is_public ? kPublic : kPrivate);
  output->append(" ");
  output->append(kKeyInfoEndMarker);
  output->append("\n");

  return true;
}

bool Extension::InitFromValue(const DictionaryValue& source, bool require_id,
                              std::string* error) {
  if (source.HasKey(kPublicKeyKey)) {
    std::string public_key_bytes;
    if (!source.GetString(kPublicKeyKey, &public_key_) ||
      !ParsePEMKeyBytes(public_key_, &public_key_bytes) ||
      !GenerateIdFromPublicKey(public_key_bytes, &id_)) {
        *error = kInvalidKeyError;
        return false;
    }
  } else if (require_id) {
    *error = kInvalidKeyError;
    return false;
  } else {
    // Generate a random ID
    id_ = StringPrintf("%x", NextGeneratedId());

    // pad the string out to kIdSize*2 chars with zeroes.
    id_.insert(0, Extension::kIdSize*2 - id_.length(), '0');

    // Convert to our mp-decimal.
    ConvertHexadecimalToIDAlphabet(&id_);
  }

  // Initialize the URL.
  extension_url_ = GURL(std::string(chrome::kExtensionScheme) +
                        chrome::kStandardSchemeSeparator + id_ + "/");

  // Initialize version.
  std::string version_str;
  if (!source.GetString(kVersionKey, &version_str)) {
    *error = kInvalidVersionError;
    return false;
  }
  version_.reset(Version::GetVersionFromString(version_str));
  if (!version_.get() || version_->components().size() > 4) {
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

  // Initialize update url (if present).
  if (source.HasKey(kUpdateURLKey)) {
    std::string tmp;
    if (!source.GetString(kUpdateURLKey, &tmp)) {
      *error =
          ExtensionErrorUtils::FormatErrorMessage(kInvalidUpdateURLError, "");
      return false;
    }
    update_url_ = GURL(tmp);
    if (!update_url_.is_valid() || update_url_.has_ref()) {
      *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidUpdateURLError,
          tmp);
      return false;
    }
  }

  // Initialize themes.
  is_theme_ = false;
  if (source.HasKey(kThemeKey)) {
    // Themes cannot contain extension keys.
    if (ContainsNonThemeKeys(source)) {
      *error = kThemesCannotContainExtensionsError;
      return false;
    }

    DictionaryValue* theme_value;
    if (!source.GetDictionary(kThemeKey, &theme_value)) {
      *error = kInvalidThemeError;
      return false;
    }
    is_theme_ = true;

    DictionaryValue* images_value;
    if (theme_value->GetDictionary(kThemeImagesKey, &images_value)) {
      // Validate that the images are all strings
      DictionaryValue::key_iterator iter = images_value->begin_keys();
      while (iter != images_value->end_keys()) {
        std::string val;
        if (!images_value->GetString(*iter, &val)) {
          *error = kInvalidThemeImagesError;
          return false;
        }
        ++iter;
      }
      theme_images_.reset(
          static_cast<DictionaryValue*>(images_value->DeepCopy()));
    }

    DictionaryValue* colors_value;
    if (theme_value->GetDictionary(kThemeColorsKey, &colors_value)) {
      // Validate that the colors are all three-item lists
      DictionaryValue::key_iterator iter = colors_value->begin_keys();
      while (iter != colors_value->end_keys()) {
        std::string val;
        int color = 0;
        ListValue* color_list;
        if (colors_value->GetList(*iter, &color_list)) {
          if (color_list->GetSize() == 3 ||
              color_list->GetSize() == 4) {
            if (color_list->GetInteger(0, &color) &&
                color_list->GetInteger(1, &color) &&
                color_list->GetInteger(2, &color)) {
              if (color_list->GetSize() == 4) {
                double alpha;
                if (color_list->GetReal(3, &alpha)) {
                  ++iter;
                  continue;
                }
              } else {
                ++iter;
                continue;
              }
            }
          }
        }
        *error = kInvalidThemeColorsError;
        return false;
        ++iter;
      }
      theme_colors_.reset(
          static_cast<DictionaryValue*>(colors_value->DeepCopy()));
    }

    DictionaryValue* tints_value;
    if (theme_value->GetDictionary(kThemeTintsKey, &tints_value)) {
      // Validate that the tints are all reals.
      DictionaryValue::key_iterator iter = tints_value->begin_keys();
      while (iter != tints_value->end_keys()) {
        ListValue* tint_list;
        double hue = 0;
        if (!tints_value->GetList(*iter, &tint_list) ||
            tint_list->GetSize() != 3 ||
            !tint_list->GetReal(0, &hue) ||
            !tint_list->GetReal(1, &hue) ||
            !tint_list->GetReal(2, &hue)) {
          *error = kInvalidThemeTintsError;
          return false;
        }
        ++iter;
      }
      theme_tints_.reset(
          static_cast<DictionaryValue*>(tints_value->DeepCopy()));
    }

    DictionaryValue* display_properties_value;
    if (theme_value->GetDictionary(kThemeDisplayPropertiesKey,
        &display_properties_value)) {
      theme_display_properties_.reset(
          static_cast<DictionaryValue*>(display_properties_value->DeepCopy()));
    }

    return true;
  }

  // Initialize plugins (optional).
  if (source.HasKey(kPluginsKey)) {
    ListValue* list_value;
    if (!source.GetList(kPluginsKey, &list_value)) {
      *error = kInvalidPluginsError;
      return false;
    }

    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      DictionaryValue* plugin_value;
      std::string path;
      bool is_public = false;

      if (!list_value->GetDictionary(i, &plugin_value)) {
        *error = kInvalidPluginsError;
        return false;
      }

      // Get plugins[i].path.
      if (!plugin_value->GetString(kPluginsPathKey, &path)) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidPluginsPathError, IntToString(i));
        return false;
      }

      // Get plugins[i].content (optional).
      if (plugin_value->HasKey(kPluginsPublicKey)) {
        if (!plugin_value->GetBoolean(kPluginsPublicKey, &is_public)) {
          *error = ExtensionErrorUtils::FormatErrorMessage(
              kInvalidPluginsPublicError, IntToString(i));
          return false;
        }
      }

      plugins_.push_back(PluginInfo());
      plugins_.back().path = path_.AppendASCII(path);
      plugins_.back().is_public = is_public;
    }
  }

  // Initialize background url (optional).
  if (source.HasKey(kBackgroundKey)) {
    std::string background_str;
    if (!source.GetString(kBackgroundKey, &background_str)) {
      *error = kInvalidBackgroundError;
      return false;
    }
    background_url_ = GetResourceURL(background_str);
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
        *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidToolstripError,
            IntToString(i));
        return false;
      }
      toolstrips_.push_back(toolstrip);
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
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidContentScriptError, IntToString(i));
        return false;
      }

      UserScript script;
      if (!LoadUserScriptHelper(content_script, i, error, &script))
        return false;  // Failed to parse script context definition
      script.set_extension_id(id());
      content_scripts_.push_back(script);
    }
  }

  // Initialize page actions (optional).
  if (source.HasKey(kPageActionsKey)) {
    ListValue* list_value;
    if (!source.GetList(kPageActionsKey, &list_value)) {
      *error = kInvalidPageActionsListError;
      return false;
    }

    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      DictionaryValue* page_action_value;
      if (!list_value->GetDictionary(i, &page_action_value)) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidPageActionError, IntToString(i));
        return false;
      }

      PageAction* page_action =
          LoadPageActionHelper(page_action_value, i, error);
      if (!page_action)
        return false;  // Failed to parse page action definition.
      page_actions_[page_action->id()] = page_action;
    }
  }

  // Initialize the permissions (optional).
  if (source.HasKey(kPermissionsKey)) {
    ListValue* hosts = NULL;
    if (!source.GetList(kPermissionsKey, &hosts)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(
          kInvalidPermissionsError, "");
      return false;
    }

    if (hosts->GetSize() == 0) {
      ExtensionErrorReporter::GetInstance()->ReportError(
          kInvalidPermissionCountWarning, false);
    }

    for (size_t i = 0; i < hosts->GetSize(); ++i) {
      std::string host_str;
      if (!hosts->GetString(i, &host_str)) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidPermissionError, IntToString(i));
        return false;
      }

      URLPattern pattern;
      if (!pattern.Parse(host_str)) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidPermissionError, IntToString(i));
        return false;
      }

      // Only accept http/https persmissions at the moment.
      if ((pattern.scheme() != chrome::kHttpScheme) &&
          (pattern.scheme() != chrome::kHttpsScheme)) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidPermissionSchemeError, IntToString(i));
        return false;
      }

      permissions_.push_back(pattern);
    }
  }

  return true;
}

std::set<FilePath> Extension::GetBrowserImages() {
  std::set<FilePath> image_paths;

  DictionaryValue* theme_images = GetThemeImages();
  if (theme_images) {
    for (DictionaryValue::key_iterator it = theme_images->begin_keys();
         it != theme_images->end_keys(); ++it) {
      std::wstring val;
      if (theme_images->GetString(*it, &val)) {
        image_paths.insert(FilePath::FromWStringHack(val));
      }
    }
  }

  for (PageActionMap::const_iterator it = page_actions().begin();
       it != page_actions().end(); ++it) {
    const std::vector<FilePath>& icon_paths = it->second->icon_paths();
    for (std::vector<FilePath>::const_iterator iter = icon_paths.begin();
         iter != icon_paths.end(); ++iter) {
      image_paths.insert(*iter);
    }
  }

  return image_paths;
}
