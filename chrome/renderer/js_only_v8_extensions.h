// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_JS_ONLY_V8_EXTENSIONS_H_
#define CHROME_RENDERER_JS_ONLY_V8_EXTENSIONS_H_

#include "v8/include/v8.h"

// This file contains various V8 Extensions that are JavaScript only, and
// don't have any C++ native functions.

class BaseJsV8Extension {
 public:
  static const char* kName;
  static v8::Extension* Get();
};

class JsonSchemaJsV8Extension {
 public:
  static const char* kName;
  static v8::Extension* Get();
};

#endif  // CHROME_RENDERER_JS_ONLY_V8_EXTENSIONS_H_
