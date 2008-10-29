// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/bitmap_platform_device_linux.h"

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "base/logging.h"

namespace gfx {

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDeviceLinux* BitmapPlatformDeviceLinux::Create(
    int width, int height, bool is_opaque) {
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, width, height);
  if (!pixbuf)
    return NULL;

  DCHECK_EQ(gdk_pixbuf_get_colorspace(pixbuf), GDK_COLORSPACE_RGB);
  DCHECK_EQ(gdk_pixbuf_get_bits_per_sample(pixbuf), 8);
  DCHECK(gdk_pixbuf_get_has_alpha(pixbuf));
  DCHECK_EQ(gdk_pixbuf_get_n_channels(pixbuf), 4);
  DCHECK_EQ(gdk_pixbuf_get_width(pixbuf), width);
  DCHECK_EQ(gdk_pixbuf_get_height(pixbuf), height);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height,
                   gdk_pixbuf_get_rowstride(pixbuf));
  bitmap.setPixels(gdk_pixbuf_get_pixels(pixbuf));
  bitmap.setIsOpaque(is_opaque);

#ifndef NDEBUG
  if (is_opaque) {
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
  }
#endif

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDeviceLinux(bitmap, pixbuf);
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(const SkBitmap& bitmap,
                                                     GdkPixbuf* pixbuf)
    : PlatformDeviceLinux(bitmap),
      pixbuf_(pixbuf) {
}

BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(
    const BitmapPlatformDeviceLinux& other)
    : PlatformDeviceLinux(const_cast<BitmapPlatformDeviceLinux&>(
                          other).accessBitmap(true)) {
}

BitmapPlatformDeviceLinux::~BitmapPlatformDeviceLinux() {
  if (pixbuf_) {
    g_object_unref(pixbuf_);
    pixbuf_ = NULL;
  }
}

}  // namespace gfx
