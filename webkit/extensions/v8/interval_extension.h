// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The IntervalExtension is a v8 extension to implement a simple interval
// class for measuring microsecond intervals.

#ifndef WEBKIT_EXTENSIONS_V8_INTERVAL_EXTENSION_H_
#define WEBKIT_EXTENSIONS_V8_INTERVAL_EXTENSION_H_

#include "v8/include/v8.h"

namespace extensions_v8 {

class IntervalExtension {
 public:
  static v8::Extension* Get();
};

}  // namespace extensions_v8

#endif  // WEBKIT_EXTENSIONS_V8_INTERVAL_EXTENSION_H_

