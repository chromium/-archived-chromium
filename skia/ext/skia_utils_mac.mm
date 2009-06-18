// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_mac.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_ptr.h"
#include "skia/ext/bitmap_platform_device_mac.h"
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

namespace gfx {

CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix) {
  // CGAffineTransforms don't support perspective transforms, so make sure
  // we don't get those.
  DCHECK(matrix[SkMatrix::kMPersp0] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp1] == 0.0f);
  DCHECK(matrix[SkMatrix::kMPersp2] == 1.0f);

  return CGAffineTransformMake(matrix[SkMatrix::kMScaleX],
                               matrix[SkMatrix::kMSkewY],
                               matrix[SkMatrix::kMSkewX],
                               matrix[SkMatrix::kMScaleY],
                               matrix[SkMatrix::kMTransX],
                               matrix[SkMatrix::kMTransY]);
}

SkIRect CGRectToSkIRect(const CGRect& rect) {
  SkIRect sk_rect = {
    SkScalarRound(rect.origin.x),
    SkScalarRound(rect.origin.y),
    SkScalarRound(rect.origin.x + rect.size.width),
    SkScalarRound(rect.origin.y + rect.size.height)
  };
  return sk_rect;
}

SkRect CGRectToSkRect(const CGRect& rect) {
  SkRect sk_rect = {
    rect.origin.x,
    rect.origin.y,
    rect.origin.x + rect.size.width,
    rect.origin.y + rect.size.height,
  };
  return sk_rect;
}

CGRect SkIRectToCGRect(const SkIRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

CGRect SkRectToCGRect(const SkRect& rect) {
  CGRect cg_rect = {
    { rect.fLeft, rect.fTop },
    { rect.fRight - rect.fLeft, rect.fBottom - rect.fTop }
  };
  return cg_rect;
}

// Converts CGColorRef to the ARGB layout Skia expects.
SkColor CGColorRefToSkColor(CGColorRef color) {
  DCHECK(CGColorGetNumberOfComponents(color) == 4);
  const CGFloat *components = CGColorGetComponents(color);
  return SkColorSetARGB(SkScalarRound(255.0 * components[3]), // alpha
                        SkScalarRound(255.0 * components[0]), // red
                        SkScalarRound(255.0 * components[1]), // green
                        SkScalarRound(255.0 * components[2])); // blue
}

// Converts ARGB to CGColorRef.
CGColorRef SkColorToCGColorRef(SkColor color) {
  return CGColorCreateGenericRGB(SkColorGetR(color) / 255.0,
                                 SkColorGetG(color) / 255.0,
                                 SkColorGetB(color) / 255.0,
                                 SkColorGetA(color) / 255.0);
}

SkBitmap CGImageToSkBitmap(CGImageRef image) {
  DCHECK(image != NULL);

  int width = CGImageGetWidth(image);
  int height = CGImageGetHeight(image);

  scoped_ptr<skia::BitmapPlatformDevice> device(
      skia::BitmapPlatformDevice::Create(NULL, width, height, false));

  CGContextRef context = device->GetBitmapContext();

  // We need to invert the y-axis of the canvas so that Core Graphics drawing
  // happens right-side up. Skia has an upper-left origin and CG has a lower-
  // left one.
  CGContextScaleCTM(context, 1.0, -1.0);
  CGContextTranslateCTM(context, 1, -height);

  // We want to copy transparent pixels from |image|, instead of blending it
  // onto uninitialized pixels.
  CGContextSetBlendMode(context, kCGBlendModeCopy);

  CGRect rect = CGRectMake(0, 0, width, height);
  CGContextDrawImage(context, rect, image);

  // Because |device| will be cleaned up and will take its pixels with it, we
  // copy it to the stack and return it.
  SkBitmap bitmap = device->accessBitmap(false);

  return bitmap;
}

SkBitmap NSImageToSkBitmap(NSImage* image, NSSize size, bool is_opaque) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width, size.height);
  if (bitmap.allocPixels() != true)
    return bitmap;  // Return |bitmap| which should respond true to isNull().

  bitmap.setIsOpaque(is_opaque);

  scoped_cftyperef<CGColorSpaceRef> color_space(
    CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  void* data = bitmap.getPixels();

  // Allocate a bitmap context with 4 components per pixel (BGRA). Apple
  // recommends these flags for improved CG performance.
  scoped_cftyperef<CGContextRef> context(
    CGBitmapContextCreate(data, size.width, size.height, 8, size.width*4,
                          color_space,
                          kCGImageAlphaPremultipliedFirst |
                              kCGBitmapByteOrder32Host));

  // Something went really wrong. Best guess is that the bitmap data is invalid.
  DCHECK(context != NULL);

  // Save the current graphics context so that we can restore it later.
  NSGraphicsContext* current_context = [NSGraphicsContext currentContext];

  // Dummy context that we will draw into.
  NSGraphicsContext* context_cocoa =
    [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
  [NSGraphicsContext setCurrentContext:context_cocoa];

  // This will stretch any images to |size| if it does not fit or is non-square.
  [image drawInRect:NSMakeRect(0, 0, size.width, size.height)
           fromRect:NSZeroRect
          operation:NSCompositeCopy
           fraction:1.0];

  // Done drawing, restore context.
  [NSGraphicsContext setCurrentContext:current_context];

  return bitmap;
}

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
  CFRelease(cgimage);
  NSImage* image = [[[NSImage alloc] init] autorelease];
  [image addRepresentation:bitmap];
  return image;
}

}  // namespace gfx
