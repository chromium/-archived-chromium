// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_BITMAP_PLATFORM_DEVICE_MAC_H_
#define SKIA_EXT_BITMAP_PLATFORM_DEVICE_MAC_H_

#include "skia/ext/platform_device_mac.h"

namespace skia {

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. Our device provides a surface CoreGraphics can also
// write to. BitmapPlatformDevice creates a bitmap using
// CGCreateBitmapContext() in a format that Skia supports and can then use this
// to draw text into, etc. This pixel data is provided to the bitmap that the
// device contains so that it can be shared.
//
// The device owns the pixel data, when the device goes away, the pixel data
// also becomes invalid. THIS IS DIFFERENT THAN NORMAL SKIA which uses
// reference counting for the pixel data. In normal Skia, you could assign
// another bitmap to this device's bitmap and everything will work properly.
// For us, that other bitmap will become invalid as soon as the device becomes
// invalid, which may lead to subtle bugs. Therefore, DO NOT ASSIGN THE
// DEVICE'S PIXEL DATA TO ANOTHER BITMAP, make sure you copy instead.
class BitmapPlatformDevice : public PlatformDevice {
 public:
  // |context| may be NULL.
  static BitmapPlatformDevice* Create(CGContextRef context,
                                      int width, int height,
                                      bool is_opaque);

  // Creates a context for |data| and calls Create.
  static BitmapPlatformDevice* CreateWithData(uint8_t* data,
                                              int width, int height,
                                              bool is_opaque);

  // Copy constructor. When copied, devices duplicate their internal data, so
  // stay linked. This is because their implementation is very heavyweight
  // (lots of memory and CoreGraphics state). If a device has been copied, both
  // clip rects and other state will stay in sync.
  //
  // This means it will NOT work to duplicate a device and assign it to a
  // canvas, because the two canvases will each set their own clip rects, and
  // the resulting CoreGraphics drawing state will be unpredictable.
  //
  // Copy constucting and "=" is designed for saving the device or passing it
  // around to another routine willing to deal with the bitmap data directly.
  BitmapPlatformDevice(const BitmapPlatformDevice& other);
  virtual ~BitmapPlatformDevice();

  // See warning for copy constructor above.
  BitmapPlatformDevice& operator=(const BitmapPlatformDevice& other);

  virtual CGContextRef GetBitmapContext();
  virtual void setMatrixClip(const SkMatrix& transform, const SkRegion& region);

  virtual void DrawToContext(CGContextRef context, int x, int y,
                             const CGRect* src_rect);
  virtual bool IsVectorial() { return false; }

  // Returns the color value at the specified location. This does not
  // consider any transforms that may be set on the device.
  SkColor getColorAt(int x, int y);

 protected:
  // Reference counted data that can be shared between multiple devices. This
  // allows copy constructors and operator= for devices to work properly. The
  // bitmaps used by the base device class are already refcounted and copyable.
  class BitmapPlatformDeviceData;

  BitmapPlatformDevice(BitmapPlatformDeviceData* data,
                       const SkBitmap& bitmap);

  // Flushes the CoreGraphics context so that the pixel data can be accessed
  // directly by Skia. Overridden from SkDevice, this is called when Skia
  // starts accessing pixel data.
  virtual void onAccessBitmap(SkBitmap*);

  virtual void processPixels(int x, int y,
                             int width, int height,
                             adjustAlpha adjustor);

  // Data associated with this device, guaranteed non-null. We hold a reference
  // to this object.
  BitmapPlatformDeviceData* data_;
};

}  // namespace skia

#endif  // SKIA_EXT_BITMAP_PLATFORM_DEVICE_MAC_H_
