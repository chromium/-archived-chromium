// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The IntervalExtension is a v8 extension to implement a simple interval
// class for measuring microsecond intervals.

#ifndef INTERVAL_EXTENSION_H__
#define INTERVAL_EXTENSION_H__

#include "v8.h"

namespace WebCore {

class IntervalExtension {
 public:
  static v8::Extension* Get();
};

}

#endif  // INTERVAL_EXTENSION_H__