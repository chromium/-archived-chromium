// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>
#include "third_party/skia/include/core/SkBitmap.h"

// Generic Cocoa utilities which fit nowhere else.

namespace CocoaUtils {

// Given an SkBitmap, return an autoreleased NSImage.  This function
// was not placed in
// third_party/skia/src/utils/mac/SkCreateCGImageRef.cpp since that
// would make Skia dependent on Cocoa.
NSImage* SkBitmapToNSImage(const SkBitmap& icon);


};  // namespace CocoaUtils
