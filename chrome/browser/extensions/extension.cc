// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension.h"

#include "base/logging.h"
#include "base/string_util.h"

const std::wstring Extension::kFormatVersionKey(L"format_version");
const std::wstring Extension::kIdKey(L"id");
const std::wstring Extension::kNameKey(L"name");
const std::wstring Extension::kDescriptionKey(L"description");
const std::wstring Extension::kContentScriptsKey(L"content_scripts");

const std::wstring Extension::kInvalidFormatVersionError(
    StringPrintf(L"Required key '%ls' is missing or invalid",
                 kFormatVersionKey.c_str()));
const std::wstring Extension::kInvalidIdError(
    StringPrintf(L"Required key '%ls' is missing or invalid.",
                 kIdKey.c_str()));
const std::wstring Extension::kInvalidNameError(
    StringPrintf(L"Required key '%ls' is missing or has invalid type.",
                 kNameKey.c_str()));
const std::wstring Extension::kInvalidDescriptionError(
    StringPrintf(L"Invalid type for '%ls' key.",
                 kDescriptionKey.c_str()));
const std::wstring Extension::kInvalidContentScriptsListError(
    StringPrintf(L"Invalid type for '%ls' key.",
                 kContentScriptsKey.c_str()));
const std::wstring Extension::kInvalidContentScriptError(
    StringPrintf(L"Invalid type for %ls at index ",
                 kContentScriptsKey.c_str()));

bool Extension::InitFromValue(const DictionaryValue& source,
                              std::wstring* error) {
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
      std::wstring content_script;
      if (!list_value->Get(i, &value) || !value->GetAsString(&content_script)) {
        *error = kInvalidContentScriptError;
        *error += IntToWString(i);
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
