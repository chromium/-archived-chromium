// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_GFX_BITMAP_PLATFORM_DEVICE_MAC_H__
#define BASE_GFX_BITMAP_PLATFORM_DEVICE_MAC_H__

#include "base/gfx/platform_device_mac.h"
#include "base/ref_counted.h"

namespace gfx {

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. Our device provides a surface CoreGraphics can also
// write to. BitmapPlatformDeviceMac creates a bitmap using
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
class BitmapPlatformDeviceMac : public PlatformDeviceMac {
 public:
  // Factory function. The screen DC is used to create the bitmap, and will not
  // be stored beyond this function. is_opaque should be set if the caller
  // knows the bitmap will be completely opaque and allows some optimizations.
  //
  // The shared_section parameter is optional (pass NULL for default behavior).
  // If shared_section is non-null, then it must be a handle to a file-mapping
  // object returned by CreateFileMapping.  See CreateDIBSection for details.
  static BitmapPlatformDeviceMac* Create(CGContextRef context,
                                         int width,
                                         int height,
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
  BitmapPlatformDeviceMac(const BitmapPlatformDeviceMac& other);
  virtual ~BitmapPlatformDeviceMac();

  // See warning for copy constructor above.
  BitmapPlatformDeviceMac& operator=(const BitmapPlatformDeviceMac& other);

  virtual CGContextRef GetBitmapContext();
  virtual void SetTransform(const SkMatrix& matrix);
  virtual void SetDeviceOffset(int x, int y);

  // This currently only supports extremely simple clip rects.
  virtual void SetClipRegion(const SkRegion& region);

  virtual void DrawToContext(CGContextRef context, int x, int y,
                             const CGRect* src_rect);
  virtual bool IsVectorial() { return false; }

  // Returns the color value at the specified location. This does not
  // consider any transforms that may be set on the device.
  SkColor GetColorAt(int x, int y);

 protected:
  // Reference counted data that can be shared between multiple devices. This
  // allows copy constructors and operator= for devices to work properly. The
  // bitmaps used by the base device class are already refcounted and copyable.
  class BitmapPlatformDeviceMacData;

  BitmapPlatformDeviceMac(BitmapPlatformDeviceMacData* data,
                          const SkBitmap& bitmap);

  // Flushes the CoreGraphics context so that the pixel data can be accessed
  // directly by Skia. Overridden from SkDevice, this is called when Skia
  // starts accessing pixel data.
  virtual void onAccessBitmap(SkBitmap*);

  // Data associated with this device, guaranteed non-null.
  scoped_refptr<BitmapPlatformDeviceMacData> data_;

  virtual void processPixels(int x, int y,
                             int width, int height, 
                             adjustAlpha adjustor);
};

}  // namespace gfx

#endif  // BASE_GFX_BITMAP_PLATFORM_DEVICE_MAC_H__
