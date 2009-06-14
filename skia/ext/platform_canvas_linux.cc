// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include <cairo/cairo.h>

#include "skia/ext/platform_device_linux.h"
#include "skia/ext/bitmap_platform_device_linux.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

PlatformCanvas::PlatformCanvas() : SkCanvas() {
}

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque)
    : SkCanvas() {
  if (!initialize(width, height, is_opaque))
    SK_CRASH();
}

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque,
                               uint8_t* data)
    : SkCanvas() {
  if (!initialize(width, height, is_opaque, data))
    SK_CRASH();
}

PlatformCanvas::~PlatformCanvas() {
}
bool PlatformCanvas::initialize(int width, int height, bool is_opaque,
                                uint8_t* data) {
  SkDevice* device =
      BitmapPlatformDevice::Create(width, height, is_opaque, data);
  if (!device)
    return false;

  setDevice(device);
  device->unref(); // was created with refcount 1, and setDevice also refs
  return true;
}

cairo_surface_t* PlatformCanvas::beginPlatformPaint() {
  return getTopPlatformDevice().beginPlatformPaint();
}

void PlatformCanvas::endPlatformPaint() {
  // We don't need to do anything on Linux here.
}

SkDevice* PlatformCanvas::createDevice(SkBitmap::Config config,
                                       int width,
                                       int height,
                                       bool is_opaque,
                                       bool isForLayer) {
  SkASSERT(config == SkBitmap::kARGB_8888_Config);
  return BitmapPlatformDevice::Create(width, height, is_opaque);
}

}  // namespace skia
