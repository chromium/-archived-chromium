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

NSImage* SkBitmapToNSImage(const SkBitmap& icon) {
  // First convert SkBitmap to CGImageRef.
  CGImageRef cgimage;
  if (icon.config() != SkBitmap::kARGB_8888_Config) {
    cgimage = SkCreateCGImageRef(icon);
  } else {
    // The above code returns a valid NSImage even in the
    // kARGB_8888_Config case.  As an example, the unit test which
    // draws a blue SkBitmap can lockPixels, NSReadPixel, and pull out
    // a single pixel from the NSImage and see it blue.  However, the
    // NSImage returned will be in ABGR format.  Although Cocoa is
    // otherwise happy with that format (as seen in simple tests
    // outside Chromium), Chromium is NOT happy.  In Chromium, B and R
    // are swapped.
    //
    // As a hint, CIImage supports a few formats, such as ARGB.
    // Interestingly, it does NOT support ABGR.  I speculate there is
    // some way we set up our drawing context which has the format
    // specified wrong (in skia/ext/bitmap_platform_device_mac.cc),
    // but I have not been able to solve this yet.
    //
    // TODO(jrg): track down the disconnect.
    // TODO(jrg): Remove byte conversion.
    // TODO(jrg): Fix unit tests to NOT swap bytes.
    // http://crbug.com/14020
    CGBitmapInfo info = (kCGImageAlphaPremultipliedFirst |
                         kCGBitmapByteOrder32Host);
    int width = icon.width();
    int height = icon.height();
    int rowbytes = icon.rowBytes();
    int rowwords = rowbytes/4;
    unsigned length = rowbytes * height;
    DCHECK(length > 0);
    uint32_t* rawptr = static_cast<uint32_t*>(malloc(length));
    DCHECK(rawptr);
    if (!rawptr || !length)
      return nil;

    // Convert ABGR to ARGB
    icon.lockPixels();
    uint32_t* rawbitmap = static_cast<uint32_t*>(icon.getPixels());
    uint32_t rawbit;
    int offset;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        offset = x + y*rowwords;
        rawbit = rawbitmap[offset];
        rawptr[offset] = SkPackARGB32(SkGetPackedA32(rawbit),
                                      SkGetPackedR32(rawbit),
                                      SkGetPackedG32(rawbit),
                                      SkGetPackedB32(rawbit));
      }
    }
    icon.unlockPixels();

    CGDataProviderRef dataRef =
      CGDataProviderCreateWithData(rawptr, rawptr, length, ReleaseData);
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    cgimage = CGImageCreate(width, height, 8,
                            icon.bytesPerPixel() * 8,
                            rowbytes, space, info, dataRef,
                            NULL, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(space);
    CGDataProviderRelease(dataRef);
  }

  // Now convert to NSImage.
  NSBitmapImageRep* bitmap = [[[NSBitmapImageRep alloc]
                                   initWithCGImage:cgimage] autorelease];
  NSImage* image = [[[NSImage alloc] init] autorelease];
  [image addRepresentation:bitmap];
  return image;
}

}  // namespace CocoaUtils

