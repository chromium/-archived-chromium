// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_

#include "v8/include/v8.h"

#include <string>

class ListValue;
class RenderThreadBase;

// This class adds extension-related javascript bindings to a renderer.  It is
// used by both web renderers and extension processes.
class RendererExtensionBindings {
 public:
  // Name of extension, for dependencies.
  static const char* kName;

  // Creates an instance of the extension.
  static v8::Extension* Get();

  // Call the given javascript function with the specified arguments.
  static void Invoke(const std::string& function_name, const ListValue& args);
};

#endif  // CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
