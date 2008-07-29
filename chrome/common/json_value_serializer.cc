// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
