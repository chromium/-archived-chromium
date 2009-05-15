// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_UNPACKER_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_UNPACKER_H_

#include <string>

#include "base/file_path.h"

class DictionaryValue;

// Implements IO for the ExtensionsService.
// TODO(aa): Extract an interface out of this for testing the frontend, once the
// frontend has significant logic to test.
class ExtensionUnpacker {
 public:
  explicit ExtensionUnpacker(const FilePath& extension_path)
      : extension_path_(extension_path) {}

  // Install the extension file at |extension_path|.  Returns true on success.
  // Otherwise, error_message will contain a string explaining what went wrong.
  bool Run();

  const std::string& error_message() { return error_message_; }

 private:
  // Read the manifest from the front of the extension file.
  // Caller takes ownership of return value.
  DictionaryValue* ReadManifest();

  // Set the error message.
  void SetError(const std::string& error);

  // The extension to unpack.
  FilePath extension_path_;

  // The last error message that was set.  Empty if there were no errors.
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUnpacker);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_UNPACKER_H_
