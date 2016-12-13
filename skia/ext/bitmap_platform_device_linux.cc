// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device_linux.h"

#include <cairo/cairo.h>

namespace skia {

// -----------------------------------------------------------------------------
// These objects are reference counted and own a Cairo surface. The surface is
// the backing store for a Skia bitmap and we reference count it so that we can
// copy BitmapPlatformDevice objects without having to copy all the image
// data.
// -----------------------------------------------------------------------------
class BitmapPlatformDevice::BitmapPlatformDeviceData
    : public base::RefCounted<BitmapPlatformDeviceData> {
 public:
  explicit BitmapPlatformDeviceData(cairo_surface_t* surface)
      : surface_(surface) { }

  cairo_surface_t* surface() const { return surface_; }

 protected:
  cairo_surface_t *const surface_;

  friend class base::RefCounted<BitmapPlatformDeviceData>;
  ~BitmapPlatformDeviceData() {
    cairo_surface_destroy(surface_);
  }

  // Disallow copy & assign.
  BitmapPlatformDeviceData(const BitmapPlatformDeviceData&);
  BitmapPlatformDeviceData& operator=(
      const BitmapPlatformDeviceData&);
};

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque,
                                                   cairo_surface_t* surface) {
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
  return new BitmapPlatformDevice
      (bitmap, new BitmapPlatformDeviceData(surface));
}

BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque) {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                 width, height);

  return Create(width, height, is_opaque, surface);
}

BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque,
                                                   uint8_t* data) {
  cairo_surface_t* surface = cairo_image_surface_create_for_data(
      data, CAIRO_FORMAT_ARGB32, width, height,
      cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width));

  return Create(width, height, is_opaque, surface);
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDevice::BitmapPlatformDevice(
    const SkBitmap& bitmap,
    BitmapPlatformDeviceData* data)
    : PlatformDevice(bitmap),
      data_(data) {
}

BitmapPlatformDevice::BitmapPlatformDevice(
    const BitmapPlatformDevice& other)
    : PlatformDevice(const_cast<BitmapPlatformDevice&>(
                          other).accessBitmap(true)),
      data_(other.data_) {
}

BitmapPlatformDevice::~BitmapPlatformDevice() {
}

cairo_surface_t* BitmapPlatformDevice::surface() const {
  return data_->surface();
}

BitmapPlatformDevice& BitmapPlatformDevice::operator=(
    const BitmapPlatformDevice& other) {
  data_ = other.data_;
  return *this;
}

}  // namespace skia
