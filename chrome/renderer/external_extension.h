// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the window.external object.

#ifndef CHROME_RENDERER_EXTENSIONS_V8_EXTERNAL_EXTENSION_H_
#define CHROME_RENDERER_EXTENSIONS_V8_EXTERNAL_EXTENSION_H_

#include "v8/include/v8.h"

namespace extensions_v8 {

class ExternalExtension {
 public:
  static v8::Extension* Get();
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_EXTENSIONS_V8_EXTERNAL_EXTENSION_H_
