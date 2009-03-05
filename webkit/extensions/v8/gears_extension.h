// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The GearsExtension is a v8 extension to add a "google.gears.factory" getter
// on the page, which, when accessed, lazily inserts the gears plugin into
// the page and attaches it to the factory variable.

#ifndef WEBKIT_EXTENSIONS_V8_GEARS_EXTENSION_H_
#define WEBKIT_EXTENSIONS_V8_GEARS_EXTENSION_H_

#include "v8/include/v8.h"

namespace extensions_v8 {

class GearsExtension {
 public:
  static v8::Extension* Get();
};

}  // namespace extensions_v8

#endif  // WEBKIT_EXTENSIONS_V8_GEARS_EXTENSION_H_

