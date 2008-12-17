// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_DEVICE_MAC_H_
#define SKIA_EXT_PLATFORM_DEVICE_MAC_H_

#import <ApplicationServices/ApplicationServices.h>
#include "SkDevice.h"

class SkMatrix;
class SkPath;
class SkRegion;

namespace skia {

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. Our device provides a surface CoreGraphics can also
// write to. It also provides functionality to play well with CG drawing
// functions.
// This class is abstract and must be subclassed. It provides the basic
// interface to implement it either with or without a bitmap backend.
class PlatformDeviceMac : public SkDevice {
 public:
  // The CGContext that corresponds to the bitmap, used for CoreGraphics
  // operations drawing into the bitmap. This is possibly heavyweight, so it
  // should exist only during one pass of rendering.
  virtual CGContextRef GetBitmapContext() = 0;

  // Draws to the given graphics context. If the bitmap context doesn't exist,
  // this will temporarily create it. However, if you have created the bitmap
  // context, it will be more efficient if you don't free it until after this
  // call so it doesn't have to be created twice.  If src_rect is null, then
  // the entirety of the source device will be copied.
  virtual void DrawToContext(CGContextRef context, int x, int y,
                             const CGRect* src_rect) = 0;

  // Sets the opacity of each pixel in the specified region to be opaque.
  void makeOpaque(int x, int y, int width, int height);

  // Returns if the preferred rendering engine is vectorial or bitmap based.
  virtual bool IsVectorial() = 0;

  // On platforms where the native rendering API does not support rendering
  // into bitmaps with a premultiplied alpha channel, this call is responsible
  // for doing any fixup necessary.  It is not used on the Mac, since
  // CoreGraphics can handle premultiplied alpha just fine.
  virtual void fixupAlphaBeforeCompositing() = 0;

  // Initializes the default settings and colors in a device context.
  static void InitializeCGContext(CGContextRef context);

  // Loads a SkPath into the CG context. The path can there after be used for
  // clipping or as a stroke.
  static void LoadPathToCGContext(CGContextRef context, const SkPath& path);

  // Loads a SkRegion into the CG context.
  static void LoadClippingRegionToCGContext(CGContextRef context,
                                            const SkRegion& region,
                                            const SkMatrix& transformation);

 protected:
  // Forwards |bitmap| to SkDevice's constructor.
  PlatformDeviceMac(const SkBitmap& bitmap);

  // Loads the specified Skia transform into the device context
  static void LoadTransformToCGContext(CGContextRef context,
                                       const SkMatrix& matrix);

  // Function pointer used by the processPixels method for setting the alpha
  // value of a particular pixel.
  typedef void (*adjustAlpha)(uint32_t* pixel);

  // Loops through each of the pixels in the specified range, invoking
  // adjustor for the alpha value of each pixel.
  virtual void processPixels(int x,
                             int y,
                             int width,
                             int height,
                             adjustAlpha adjustor) = 0;
};

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_DEVICE_MAC_H_

