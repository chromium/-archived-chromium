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