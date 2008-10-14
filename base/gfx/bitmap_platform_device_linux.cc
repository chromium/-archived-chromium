// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/bitmap_platform_device_linux.h"

#include "base/logging.h"

#include <time.h>

namespace gfx {

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDeviceLinux* BitmapPlatformDeviceLinux::Create(
    int width, int height, bool is_opaque) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap.setIsOpaque(is_opaque);

  if (is_opaque) {
#ifndef NDEBUG
    // To aid in finding bugs, we set the background color to something
    // obviously wrong so it will be noticable when it is not cleared
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
#endif
  }

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDeviceLinux(bitmap);
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(const SkBitmap& bitmap)
    : PlatformDeviceLinux(bitmap) {
}

BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(
    const BitmapPlatformDeviceLinux& other)
    : PlatformDeviceLinux(const_cast<BitmapPlatformDeviceLinux&>(
                          other).accessBitmap(true)) {
}

BitmapPlatformDeviceLinux::~BitmapPlatformDeviceLinux() {
}

}  // namespace gfx
