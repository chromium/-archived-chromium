// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas_linux.h"

#include "skia/ext/platform_device_linux.h"
#include "skia/ext/bitmap_platform_device_linux.h"
#include "SkTypes.h"

#include <cairo/cairo.h>

namespace skia {

PlatformCanvasLinux::PlatformCanvasLinux() : SkCanvas() {
}

PlatformCanvasLinux::PlatformCanvasLinux(int width, int height, bool is_opaque)
    : SkCanvas() {
  if (!initialize(width, height, is_opaque))
    SK_CRASH();
}

PlatformCanvasLinux::PlatformCanvasLinux(int width, int height, bool is_opaque,
                                         uint8_t* data)
    : SkCanvas() {
  if (!initialize(width, height, is_opaque, data))
    SK_CRASH();
}

PlatformCanvasLinux::~PlatformCanvasLinux() {
}

bool PlatformCanvasLinux::initialize(int width, int height, bool is_opaque) {
  SkDevice* device = createPlatformDevice(width, height, is_opaque);
  if (!device)
    return false;

  setDevice(device);
  device->unref(); // was created with refcount 1, and setDevice also refs
  return true;
}

bool PlatformCanvasLinux::initialize(int width, int height, bool is_opaque,
                                     uint8_t* data) {
  SkDevice* device =
    BitmapPlatformDeviceLinux::Create(width, height, is_opaque, data);
  if (!device)
    return false;

  setDevice(device);
  device->unref(); // was created with refcount 1, and setDevice also refs
  return true;
}

PlatformDeviceLinux& PlatformCanvasLinux::getTopPlatformDevice() const {
  // All of our devices should be our special PlatformDevice.
  SkCanvas::LayerIter iter(const_cast<PlatformCanvasLinux*>(this), false);
  return *static_cast<PlatformDeviceLinux*>(iter.device());
}

SkDevice* PlatformCanvasLinux::createDevice(SkBitmap::Config config,
                                            int width,
                                            int height,
                                            bool is_opaque, bool isForLayer) {
  SkASSERT(config == SkBitmap::kARGB_8888_Config);
  return createPlatformDevice(width, height, is_opaque);
}

SkDevice* PlatformCanvasLinux::createPlatformDevice(int width,
                                                    int height,
                                                    bool is_opaque) {
  return BitmapPlatformDeviceLinux::Create(width, height, is_opaque);
}

// static
size_t PlatformCanvasLinux::StrideForWidth(unsigned width) {
  return cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
}

}  // namespace skia
