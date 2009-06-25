// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include "chrome/browser/cocoa/cocoa_utils.h"
#import <QuartzCore/CIImage.h>
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "third_party/skia/include/core/SkColorPriv.h"

namespace {

// Callback passed to CGDataProviderCreateWithData()
void ReleaseData(void* info, const void* pixelData, size_t size) {
  // info is a non-const pixelData
  if (info)
    free(info);
}

}

namespace CocoaUtils {

NSImage* SkBitmapToNSImage(const SkBitmap& skiaBitmap) {
  // First convert SkBitmap to CGImageRef.
  CGImageRef cgimage = SkCreateCGImageRef(skiaBitmap);

  // Now convert to NSImage.
  NSBitmapImageRep* bitmap = [[[NSBitmapImageRep alloc]
                                   initWithCGImage:cgimage] autorelease];
  CFRelease(cgimage);
  NSImage* image = [[[NSImage alloc] init] autorelease];
  [image addRepresentation:bitmap];
  return image;
}

}  // namespace CocoaUtils

