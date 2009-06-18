// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/web_resource/web_resource_unpacker.h"

#include "base/json_reader.h"
#include "base/values.h"

const char* WebResourceUnpacker::kInvalidDataTypeError =
    "Data from web resource server is missing or not valid JSON.";

const char* WebResourceUnpacker::kUnexpectedJSONFormatError =
    "Data from web resource server does not have expected format.";

// TODO(mrc): Right now, this reads JSON data from the experimental popgadget
// server.  Change so the format is based on a template, once we have
// decided on final server format.
bool WebResourceUnpacker::Run() {
  scoped_ptr<Value> value;
  if (!resource_data_.empty()) {
    value.reset(JSONReader::Read(resource_data_, false));
    if (!value.get()) {
      // Page information not properly read, or corrupted.
      error_message_ = kInvalidDataTypeError;
      return false;
    }
    if (!value->IsType(Value::TYPE_LIST)) {
      error_message_ = kUnexpectedJSONFormatError;
      return false;
    }
    parsed_json_.reset(static_cast<ListValue*>(value.release()));
    return true;
  }
  error_message_ = kInvalidDataTypeError;
  return false;
}

