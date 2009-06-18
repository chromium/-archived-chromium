// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is called by the WebResourceService in a sandboxed process
// to unpack data retrieved from a web resource feed.  Right now, it
// takes a string of data in JSON format, parses it, and hands it back
// to the WebResourceService as a list of items.  In the future
// it will be set up to unpack and verify image data in addition to
// just parsing a JSON feed.

#ifndef CHROME_COMMON_WEB_RESOURCE_UNPACKER_H_
#define CHROME_COMMON_WEB_RESOURCE_UNPACKER_H_

#include <string>

#include "base/file_path.h"
#include "base/scoped_ptr.h"

class ListValue;

class WebResourceUnpacker {
 public:
  static const char* kInvalidDataTypeError;
  static const char* kUnexpectedJSONFormatError;

  explicit WebResourceUnpacker(const std::string &resource_data)
    : resource_data_(resource_data) {}

  // This does the actual parsing.  In case of an error, error_message_
  // is set to an appropriate value.
  bool Run();

  // Returns the last error message set by Run().
  const std::string& error_message() { return error_message_; }

  // Gets data which has been parsed by Run().
  ListValue* parsed_json() {
    return parsed_json_.get();
  }

 private:
  // Holds the string which is to be parsed.
  std::string resource_data_;

  // Holds the result of JSON parsing of resource_data_.
  scoped_ptr<ListValue> parsed_json_;

  // Holds the last error message produced by Run().
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(WebResourceUnpacker);
};

#endif  // CHROME_COMMON_WEB_RESOURCE_UNPACKER_H_


