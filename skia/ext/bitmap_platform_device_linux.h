// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_BITMAP_PLATFORM_DEVICE_LINUX_H_
#define SKIA_EXT_BITMAP_PLATFORM_DEVICE_LINUX_H_

#include "base/ref_counted.h"
#include "skia/ext/platform_device_linux.h"

typedef struct _cairo_surface cairo_surface_t;

// -----------------------------------------------------------------------------
// Image byte ordering on Linux:
//
// Pixels are packed into 32-bit words these days. Even for 24-bit images,
// often 8-bits will be left unused for alignment reasons. Thus, when you see
// ARGB as the byte order you have to wonder if that's in memory order or
// little-endian order. Here I'll write A.R.G.B to specifiy the memory order.
//
// GdkPixbuf's provide a nice backing store and defaults to R.G.B.A order.
// They'll do the needed byte swapping to match the X server when drawn.
//
// Skia can be controled in skia/include/corecg/SkUserConfig.h (see bits about
// SK_R32_SHIFT). For Linux we define it to be ARGB in registers. For little
// endian machines that means B.G.R.A in memory.
//
// The image loaders are controlled in
// webkit/port/platform/image-decoders/ImageDecoder.h (see setRGBA). These are
// also configured for ARGB in registers.
//
// Cairo's only 32-bit mode is ARGB in registers.
//
// X servers commonly have a 32-bit visual with xRGB in registers (since they
// typically don't do alpha blending of drawables at the user level. Composite
// extensions aside.)
//
// We don't use GdkPixbuf because its byte order differs from the rest. Most
// importantly, it differs from Cairo which, being a system library, is
// something that we can't easily change.
// -----------------------------------------------------------------------------

namespace skia {

// -----------------------------------------------------------------------------
// This is the Linux bitmap backing for Skia. We create a Cairo image surface
// to store the backing buffer. This buffer is BGRA in memory (on little-endian
// machines).
//
// For now we are also using Cairo to paint to the Drawables so we provide an
// accessor for getting the surface.
//
// This is all quite ok for test_shell. In the future we will want to use
// shared memory between the renderer and the main process at least. In this
// case we'll probably create the buffer from a precreated region of memory.
// -----------------------------------------------------------------------------
class BitmapPlatformDevice : public PlatformDevice {
  // A reference counted cairo surface
  class BitmapPlatformDeviceData;

 public:
  static BitmapPlatformDevice* Create(int width, int height, bool is_opaque);

  // This doesn't take ownership of |data|
  static BitmapPlatformDevice* Create(int width, int height,
                                      bool is_opaque, uint8_t* data);
  static BitmapPlatformDevice* Create(int width, int height,
                                      bool is_opaque, cairo_surface_t* surface);

  // Create a BitmapPlatformDeviceLinux from an already constructed bitmap;
  // you should probably be using Create(). This may become private later if
  // we ever have to share state between some native drawing UI and Skia, like
  // the Windows and Mac versions of this class do.
  //
  // This object takes ownership of @data.
  BitmapPlatformDevice(const SkBitmap& other, BitmapPlatformDeviceData* data);
  virtual ~BitmapPlatformDevice();
  BitmapPlatformDevice& operator=(const BitmapPlatformDevice& other);

  // A stub copy constructor.  Needs to be properly implemented.
  BitmapPlatformDevice(const BitmapPlatformDevice& other);

  // Bitmaps aren't vector graphics.
  virtual bool IsVectorial() { return false; }

  // If someone wants to paint on a Cairo surface version of our
  // buffer, then give them the surface we're already using.
  virtual cairo_surface_t* beginPlatformPaint() { return surface(); }

  cairo_surface_t* surface() const;

 private:
  scoped_refptr<BitmapPlatformDeviceData> data_;
};

}  // namespace skia

#endif  // SKIA_EXT_BITMAP_PLATFORM_DEVICE_LINUX_H_
