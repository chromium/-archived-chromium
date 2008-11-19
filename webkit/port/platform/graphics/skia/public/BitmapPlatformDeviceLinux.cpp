// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/bitmap_platform_device_linux.h"

#include <cairo/cairo.h>

#include "base/logging.h"

namespace gfx {

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDeviceLinux* BitmapPlatformDeviceLinux::Create(
    int width, int height, bool is_opaque) {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                 width, height);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height,
                   cairo_image_surface_get_stride(surface));
  bitmap.setPixels(cairo_image_surface_get_data(surface));
  bitmap.setIsOpaque(is_opaque);

#ifndef NDEBUG
  if (is_opaque) {
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
  }
#endif

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDeviceLinux(bitmap, surface);
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(const SkBitmap& bitmap,
                                                     cairo_surface_t* surface)
    : PlatformDeviceLinux(bitmap),
      surface_(surface) {
}

BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(
    const BitmapPlatformDeviceLinux& other)
    : PlatformDeviceLinux(const_cast<BitmapPlatformDeviceLinux&>(
                          other).accessBitmap(true)) {
}

BitmapPlatformDeviceLinux::~BitmapPlatformDeviceLinux() {
  if (surface_) {
    cairo_surface_destroy(surface_);
    surface_ = NULL;
  }
}

}  // namespace gfx
