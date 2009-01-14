// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas_mac.h"

#include "skia/ext/bitmap_platform_device_mac.h"
#include "SkTypes.h"

namespace skia {

PlatformCanvasMac::PlatformCanvasMac() : SkCanvas() {
}

PlatformCanvasMac::PlatformCanvasMac(int width, int height, bool is_opaque)
    : SkCanvas() {
  initialize(width, height, is_opaque);
}

PlatformCanvasMac::PlatformCanvasMac(int width,
                                     int height,
                                     bool is_opaque,
                                     CGContextRef context)
    : SkCanvas() {
  initialize(width, height, is_opaque);
}

PlatformCanvasMac::~PlatformCanvasMac() {
}

bool PlatformCanvasMac::initialize(int width,
                                   int height,
                                   bool is_opaque) {
  SkDevice* device = createPlatformDevice(width, height, is_opaque, NULL);
  if (!device)
    return false;

  setDevice(device);
  device->unref(); // was created with refcount 1, and setDevice also refs
  return true;
}

CGContextRef PlatformCanvasMac::beginPlatformPaint() {
  return getTopPlatformDevice().GetBitmapContext();
}

void PlatformCanvasMac::endPlatformPaint() {
  // flushing will be done in onAccessBitmap
}

PlatformDeviceMac& PlatformCanvasMac::getTopPlatformDevice() const {
  // All of our devices should be our special PlatformDeviceMac.
  SkCanvas::LayerIter iter(const_cast<PlatformCanvasMac*>(this), false);
  return *static_cast<PlatformDeviceMac*>(iter.device());
}

SkDevice* PlatformCanvasMac::createDevice(SkBitmap::Config config,
                                          int width,
                                          int height,
                                          bool is_opaque, bool isForLayer) {
  SkASSERT(config == SkBitmap::kARGB_8888_Config);
  return createPlatformDevice(width, height, is_opaque, NULL);
}

SkDevice* PlatformCanvasMac::createPlatformDevice(int width,
                                                  int height,
                                                  bool is_opaque,
                                                  CGContextRef context) {
  SkDevice* device = BitmapPlatformDeviceMac::Create(context, width, height,
                                                     is_opaque);
  return device;
}

SkDevice* PlatformCanvasMac::setBitmapDevice(const SkBitmap&) {
  SkASSERT(false);
  return NULL;
}

}  // namespace skia

