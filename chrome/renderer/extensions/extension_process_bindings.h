// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exposes extension APIs into the extension process.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_

#include <string>
#include <vector>

#include "v8/include/v8.h"

class ExtensionProcessBindings {
 public:
  static void SetFunctionNames(const std::vector<std::string>& names);
  static v8::Extension* Get();

  // Handles a response to an API request.
  static void HandleResponse(int request_id, bool success,
                             const std::string& response,
                             const std::string& error);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
