// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exposes extension APIs into the extension process.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_

#include <string>

#include "v8/include/v8.h"

class WebFrame;

namespace extensions_v8 {

class ExtensionProcessBindings {
 public:
  static v8::Extension* Get();
  static void ExecuteCallbackInFrame(WebFrame* frame, int callback_id,
                                     const std::string& response);
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
