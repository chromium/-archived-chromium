// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/json_value_serializer.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/string_util.h"
#include "chrome/common/logging_chrome.h"

JSONStringValueSerializer::~JSONStringValueSerializer() {}

bool JSONStringValueSerializer::Serialize(const Value& root) {
  if (!json_string_ || initialized_with_const_string_)
    return false;

  JSONWriter::Write(&root, pretty_print_, json_string_);

  return true;
}

bool JSONStringValueSerializer::Deserialize(Value** root) {
  if (!json_string_)
    return false;

  return JSONReader::Read(*json_string_, root, allow_trailing_comma_);
}

/******* File Serializer *******/

bool JSONFileValueSerializer::Serialize(const Value& root) {
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.set_pretty_print(true);
  bool result = serializer.Serialize(root);
  if (!result)
    return false;

  FILE* file = NULL;
  _wfopen_s(&file, json_file_path_.c_str(), L"wb");
  if (!file)
    return false;

  size_t amount_written =
    fwrite(json_string.data(), 1, json_string.size(), file);
  fclose(file);

  return amount_written == json_string.size();
}

bool JSONFileValueSerializer::Deserialize(Value** root) {
  FILE* file = NULL;
  _wfopen_s(&file, json_file_path_.c_str(), L"rb");
  if (!file)
    return false;

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  bool result = false;
  std::string json_string;
  size_t chars_read = fread(
    // WriteInto assumes the last character is a null, and it's not in this
    // case, so we need to add 1 to our size to ensure the last character
    // doesn't get cut off.
    WriteInto(&json_string, file_size + 1), 1, file_size, file);
  if (chars_read == file_size) {
    JSONStringValueSerializer serializer(json_string);
    result = serializer.Deserialize(root);
  }

  fclose(file);
  return result;
}

