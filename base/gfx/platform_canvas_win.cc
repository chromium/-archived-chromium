// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/platform_canvas_win.h"

#include "base/gfx/bitmap_platform_device_win.h"
#include "base/logging.h"

#ifdef ARCH_CPU_64_BITS
#error This code does not work on x64. Please make sure all the base unit tests\
 pass before doing any real work.
#endif

namespace gfx {

PlatformCanvasWin::PlatformCanvasWin() : SkCanvas() {
}

PlatformCanvasWin::PlatformCanvasWin(int width, int height, bool is_opaque)
    : SkCanvas() {
  initialize(width, height, is_opaque, NULL);
}

PlatformCanvasWin::PlatformCanvasWin(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section)
    : SkCanvas() {
  initialize(width, height, is_opaque, shared_section);
}

PlatformCanvasWin::~PlatformCanvasWin() {
}

void PlatformCanvasWin::initialize(int width,
                                int height,
                                bool is_opaque,
                                HANDLE shared_section) {
  SkDevice* device =
      createPlatformDevice(width, height, is_opaque, shared_section);
  setDevice(device);
  device->unref(); // was created with refcount 1, and setDevice also refs
}

HDC PlatformCanvasWin::beginPlatformPaint() {
  return getTopPlatformDevice().getBitmapDC();
}

void PlatformCanvasWin::endPlatformPaint() {
  // we don't clear the DC here since it will be likely to be used again
  // flushing will be done in onAccessBitmap
}

PlatformDeviceWin& PlatformCanvasWin::getTopPlatformDevice() const {
  // All of our devices should be our special PlatformDevice.
  SkCanvas::LayerIter iter(const_cast<PlatformCanvasWin*>(this), false);
  return *static_cast<PlatformDeviceWin*>(iter.device());
}

SkDevice* PlatformCanvasWin::createDevice(SkBitmap::Config config,
                                       int width,
                                       int height,
                                       bool is_opaque, bool isForLayer) {
  DCHECK(config == SkBitmap::kARGB_8888_Config);
  return createPlatformDevice(width, height, is_opaque, NULL);
}

SkDevice* PlatformCanvasWin::createPlatformDevice(int width,
                                               int height,
                                               bool is_opaque,
                                               HANDLE shared_section) {
  HDC screen_dc = GetDC(NULL);
  SkDevice* device = BitmapPlatformDeviceWin::create(screen_dc, width, height,
                                                  is_opaque, shared_section);
  ReleaseDC(NULL, screen_dc);
  return device;
}

SkDevice* PlatformCanvasWin::setBitmapDevice(const SkBitmap&) {
  NOTREACHED();
  return NULL;
}

}  // namespace gfx
