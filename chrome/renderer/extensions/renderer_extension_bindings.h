// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_

#include "v8/include/v8.h"

#include <string>

class WebFrame;

namespace extensions_v8 {

// This class adds extension-related javascript bindings to a renderer.
class RendererExtensionBindings {
 public:
  static v8::Extension* Get();
  static void HandleExtensionMessage(
      WebFrame* webframe, const std::string& message, int channel_id);
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
