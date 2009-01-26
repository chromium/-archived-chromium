// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The GCExtension is a v8 extension to expose a method into JS for triggering
// garbage collection.  This should only be used for debugging.

#ifndef GC_EXTENSION_H__
#define GC_EXTENSION_H__

#include "v8.h"

namespace WebCore {

class GCExtension {
 public:
  static v8::Extension* Get();
};

}

#endif  // GC_EXTENSION_H__
