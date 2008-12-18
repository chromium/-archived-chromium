// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension.h"

#include "base/logging.h"
#include "base/string_util.h"

const FilePath::CharType* Extension::kManifestFilename =
    FILE_PATH_LITERAL("manifest");

const wchar_t* Extension::kFormatVersionKey = L"format_version";
const wchar_t* Extension::kIdKey = L"id";
const wchar_t* Extension::kNameKey = L"name";
const wchar_t* Extension::kDescriptionKey = L"description";
const wchar_t* Extension::kContentScriptsKey = L"content_scripts";

const char* Extension::kInvalidManifestError =
    "Manifest is missing or invalid.";
const char* Extension::kInvalidFormatVersionError =
    "Required key 'format_version' is missing or invalid";
const char* Extension::kInvalidIdError =
    "Required key 'id' is missing or invalid.";
const char* Extension::kInvalidNameError =
    "Required key 'name' is missing or has invalid type.";
const char* Extension::kInvalidDescriptionError =
    "Invalid type for 'description' key.";
const char* Extension::kInvalidContentScriptsListError =
    "Invalid type for 'content_scripts' key.";
const char* Extension::kInvalidContentScriptError =
    "Invalid type for content_scripts at index ";

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

  // Initialize name.
  if (!source.GetString(kNameKey, &name_)) {
    *error = kInvalidNameError;
    return false;
  }

  // Initialize description (optional).
  Value* value = NULL;
  if (source.Get(kDescriptionKey, &value)) {
    if (!value->GetAsString(&description_)) {
      *error = kInvalidDescriptionError;
      return false;
    }
  }

  // Initialize content scripts (optional).
  if (source.Get(kContentScriptsKey, &value)) {
    ListValue* list_value = NULL;
    if (value->GetType() != Value::TYPE_LIST) {
      *error = kInvalidContentScriptsListError;
      return false;
    } else {
      list_value = static_cast<ListValue*>(value);
    }

    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      std::string content_script;
      if (!list_value->Get(i, &value) || !value->GetAsString(&content_script)) {
        *error = kInvalidContentScriptError;
        *error += IntToString(i);
        return false;
      }

      content_scripts_.push_back(content_script);
    }
  }

  return true;
}

void Extension::CopyToValue(DictionaryValue* destination) {
  // Set format version
  destination->SetInteger(kFormatVersionKey,
                          kExpectedFormatVersion);

  // Copy id.
  destination->SetString(kIdKey, id_);

  // Copy name.
  destination->SetString(kNameKey, name_);

  // Copy description (optional).
  if (description_.size() > 0)
    destination->SetString(kDescriptionKey, description_);

  // Copy content scripts (optional).
  if (content_scripts_.size() > 0) {
    ListValue* list_value = new ListValue();
    destination->Set(kContentScriptsKey, list_value);

    for (size_t i = 0; i < content_scripts_.size(); ++i) {
      list_value->Set(i, Value::CreateStringValue(content_scripts_[i]));
    }
  }
}
