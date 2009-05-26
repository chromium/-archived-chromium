// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/url_constants.h"

const char Extension::kManifestFilename[] = "manifest.json";

const wchar_t* Extension::kContentScriptsKey = L"content_scripts";
const wchar_t* Extension::kCssKey = L"css";
const wchar_t* Extension::kDescriptionKey = L"description";
const wchar_t* Extension::kIconPathKey = L"icon";
const wchar_t* Extension::kIdKey = L"id";
const wchar_t* Extension::kJsKey = L"js";
const wchar_t* Extension::kMatchesKey = L"matches";
const wchar_t* Extension::kNameKey = L"name";
const wchar_t* Extension::kPageActionsKey = L"page_actions";
const wchar_t* Extension::kPermissionsKey = L"permissions";
const wchar_t* Extension::kPluginsDirKey = L"plugins_dir";
const wchar_t* Extension::kBackgroundKey = L"background_page";
const wchar_t* Extension::kRunAtKey = L"run_at";
const wchar_t* Extension::kThemeKey = L"theme";
const wchar_t* Extension::kThemeImagesKey = L"images";
const wchar_t* Extension::kThemeColorsKey = L"colors";
const wchar_t* Extension::kThemeTintsKey = L"tints";
const wchar_t* Extension::kToolstripsKey = L"toolstrips";
const wchar_t* Extension::kTooltipKey = L"tooltip";
const wchar_t* Extension::kTypeKey = L"type";
const wchar_t* Extension::kVersionKey = L"version";
const wchar_t* Extension::kZipHashKey = L"zip_hash";

const char* Extension::kRunAtDocumentStartValue = "document_start";
const char* Extension::kRunAtDocumentEndValue = "document_end";
const char* Extension::kPageActionTypeTab = "tab";
const char* Extension::kPageActionTypePermanent = "permanent";


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
const char* Extension::kInvalidPageActionError =
    "Invalid value for 'page_actions[*]'.";
const char* Extension::kInvalidPageActionsListError =
    "Invalid value for 'page_actions'.";
const char* Extension::kInvalidPageActionIconPathError =
    "Invalid value for 'page_actions[*].icon'.";
const char* Extension::kInvalidPageActionTooltipError =
    "Invalid value for 'page_actions[*].tooltip'.";
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
const char* Extension::kInvalidPluginsDirError =
    "Invalid value for 'plugins_dir'.";
const char* Extension::kInvalidBackgroundError =
    "Invalid value for 'background'.";
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

const size_t Extension::kIdSize = 20;  // SHA1 (160 bits) == 20 bytes

Extension::~Extension() {
  for (PageActionMap::iterator i = page_actions_.begin();
       i != page_actions_.end(); ++i)
    delete i->second;
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

const PageAction* Extension::GetPageAction(std::string id) const {
  PageActionMap::const_iterator it = page_actions_.find(id);
  if (it == page_actions_.end())
    return NULL;

  return it->second;
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

Extension::Extension(const FilePath& path) {
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

  // Read the page action |icon|.
  std::string icon;
  if (!page_action->GetString(kIconPathKey, &icon)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(
        kInvalidPageActionIconPathError, IntToString(definition_index));
    return NULL;
  }
  FilePath icon_path = path_.AppendASCII(icon);
  result->set_icon_path(icon_path);

  // Read the page action |id|.
  std::string id;
  if (!page_action->GetString(kIdKey, &id)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidIdError,
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

  // Read the page action |tooltip|.
  std::string tooltip;
  if (!page_action->GetString(kTooltipKey, &tooltip)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(
        kInvalidPageActionTooltipError, IntToString(definition_index));
    return NULL;
  }
  result->set_tooltip(tooltip);

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

bool Extension::InitFromValue(const DictionaryValue& source, bool require_id,
                              std::string* error) {
  // Initialize id.
  if (source.HasKey(kIdKey)) {
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
  } else if (require_id) {
    *error = kInvalidIdError;
    return false;
  } else {
    // Generate a random ID
    static int counter = 0;
    id_ = StringPrintf("%x", counter);
    ++counter;

    // pad the string out to 40 chars with zeroes.
    id_.insert(0, 40 - id_.length(), '0');
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

  // Initialize themes. If a theme is included, no other items may be processed
  // (we currently don't want people bundling themes and extension stuff
  // together).
  //
  // TODO(glen): Error if other items *are* included.
  is_theme_ = false;
  if (source.HasKey(kThemeKey)) {
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
        if (!colors_value->GetList(*iter, &color_list) ||
            color_list->GetSize() != 3 ||
            !color_list->GetInteger(0, &color) ||
            !color_list->GetInteger(1, &color) ||
            !color_list->GetInteger(2, &color)) {
          *error = kInvalidThemeColorsError;
          return false;
        }
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
    return true;
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
