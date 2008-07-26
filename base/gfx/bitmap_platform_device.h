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

#ifndef BASE_GFX_BITMAP_PLATFORM_DEVICE_H__
#define BASE_GFX_BITMAP_PLATFORM_DEVICE_H__

#include "base/gfx/platform_device.h"
#include "base/ref_counted.h"

namespace gfx {

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. Our device provides a surface Windows can also write
// to. BitmapPlatformDevice creates a bitmap using CreateDIBSection() in a
// format that Skia supports and can then use this to draw ClearType into, etc.
// This pixel data is provided to the bitmap that the device contains so that it
// can be shared.
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
  // Factory function. The screen DC is used to create the bitmap, and will not
  // be stored beyond this function. is_opaque should be set if the caller
  // knows the bitmap will be completely opaque and allows some optimizations.
  //
  // The shared_section parameter is optional (pass NULL for default behavior).
  // If shared_section is non-null, then it must be a handle to a file-mapping
  // object returned by CreateFileMapping.  See CreateDIBSection for details.
  static BitmapPlatformDevice* create(HDC screen_dc,
                                int width,
                                int height,
                                bool is_opaque,
                                HANDLE shared_section);

  // Copy constructor. When copied, devices duplicate their internal data, so
  // stay linked. This is because their implementation is very heavyweight
  // (lots of memory and some GDI objects). If a device has been copied, both
  // clip rects and other state will stay in sync.
  //
  // This means it will NOT work to duplicate a device and assign it to a
  // canvas, because the two canvases will each set their own clip rects, and
  // the resulting GDI clip rect will be random.
  //
  // Copy constucting and "=" is designed for saving the device or passing it
  // around to another routine willing to deal with the bitmap data directly.
  BitmapPlatformDevice(const BitmapPlatformDevice& other);
  virtual ~BitmapPlatformDevice();

  // See warning for copy constructor above.
  BitmapPlatformDevice& operator=(const BitmapPlatformDevice& other);

  // Retrieves the bitmap DC, which is the memory DC for our bitmap data. The
  // bitmap DC is lazy created.
  virtual HDC getBitmapDC();
  virtual void setMatrixClip(const SkMatrix& transform, const SkRegion& region);
  virtual void setDeviceOffset(int x, int y);

  virtual void drawToHDC(HDC dc, int x, int y, const RECT* src_rect);
  virtual void prepareForGDI(int x, int y, int width, int height);
  virtual void postProcessGDI(int x, int y, int width, int height);
  virtual void makeOpaque(int x, int y, int width, int height);
  virtual void fixupAlphaBeforeCompositing();
  virtual bool IsVectorial() { return false; }

  // Returns the color value at the specified location. This does not
  // consider any transforms that may be set on the device.
  SkColor getColorAt(int x, int y);

 protected:
  // Flushes the Windows device context so that the pixel data can be accessed
  // directly by Skia. Overridden from SkDevice, this is called when Skia
  // starts accessing pixel data.
  virtual void onAccessBitmap(SkBitmap* bitmap);

 private:
  // Function pointer used by the processPixels method for setting the alpha
  // value of a particular pixel.
  typedef void (*adjustAlpha)(uint32_t* pixel);

  // Reference counted data that can be shared between multiple devices. This
  // allows copy constructors and operator= for devices to work properly. The
  // bitmaps used by the base device class are already refcounted and copyable.
  class BitmapPlatformDeviceData;

  // Private constructor.
  BitmapPlatformDevice(BitmapPlatformDeviceData* data, const SkBitmap& bitmap);

  // Loops through each of the pixels in the specified range, invoking
  // adjustor for the alpha value of each pixel. If |width| or |height| are -1,
  // the available width/height is used.
  template<adjustAlpha adjustor>
  void processPixels(int x,
                     int y,
                     int width,
                     int height);

  // Data associated with this device, guaranteed non-null.
  scoped_refptr<BitmapPlatformDeviceData> data_;
};

}  // namespace gfx

#endif  // BASE_GFX_BITMAP_PLATFORM_DEVICE_H__
