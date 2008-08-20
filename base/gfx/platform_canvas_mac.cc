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

#include "base/gfx/platform_canvas_mac.h"

#include "base/gfx/bitmap_platform_device_mac.h"
#include "base/logging.h"

namespace gfx {

PlatformCanvasMac::PlatformCanvasMac() : SkCanvas() {
}

PlatformCanvasMac::PlatformCanvasMac(int width, int height, bool is_opaque)
    : SkCanvas() {
  initialize(width, height, is_opaque, NULL);
}

PlatformCanvasMac::PlatformCanvasMac(int width,
                                     int height,
                                     bool is_opaque,
                                     CGContextRef context)
    : SkCanvas() {
  initialize(width, height, is_opaque, context);
}

PlatformCanvasMac::~PlatformCanvasMac() {
}

void PlatformCanvasMac::initialize(int width,
                                   int height,
                                   bool is_opaque,
                                   CGContextRef context) {
  SkDevice* device =
      createPlatformDevice(width, height, is_opaque, context);
  setDevice(device);
  device->unref(); // was created with refcount 1, and setDevice also refs
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

// Clipping -------------------------------------------------------------------

bool PlatformCanvasMac::clipRect(const SkRect& rect, SkRegion::Op op) {
  bool ret = SkCanvas::clipRect(rect, op);
  getTopPlatformDevice().SetClipRegion(getTotalClip());
  return ret;
}

bool PlatformCanvasMac::clipPath(const SkPath& path, SkRegion::Op op) {
  bool ret = SkCanvas::clipPath(path, op);
  getTopPlatformDevice().SetClipRegion(getTotalClip());
  return ret;
}

bool PlatformCanvasMac::clipRegion(const SkRegion& deviceRgn, SkRegion::Op op) {
  bool ret = SkCanvas::clipRegion(deviceRgn, op);
  getTopPlatformDevice().SetClipRegion(getTotalClip());
  return ret;
}

// Transforming ---------------------------------------------------------------

bool PlatformCanvasMac::translate(SkScalar dx, SkScalar dy) {
  if (!SkCanvas::translate(dx, dy))
    return false;
  getTopPlatformDevice().SetTransform(getTotalMatrix());
  return true;
}

bool PlatformCanvasMac::scale(SkScalar sx, SkScalar sy) {
  if (!SkCanvas::scale(sx, sy))
    return false;
  getTopPlatformDevice().SetTransform(getTotalMatrix());
  return true;
}

int PlatformCanvasMac::saveLayer(const SkRect* bounds,
                              const SkPaint* paint,
                              SaveFlags flags) {
  int result = SkCanvas::saveLayer(bounds, paint, flags);

  // saveLayer will have created a new device which, depending on the clip
  // rect, may be smaller than the original layer. Therefore, it will have a
  // transform applied, and we need to sync CG with that transform.
  SkCanvas::LayerIter iter(this, false);
  PlatformDeviceMac& new_device =
      *static_cast<PlatformDeviceMac*>(iter.device());

  // There man not actually be a new layer if the layer is empty.
  if (!iter.done()) {
    new_device.SetDeviceOffset(iter.x(), iter.y());
    new_device.SetTransform(getTotalMatrix());
    new_device.SetClipRegion(getTotalClip());
  }
  return result;
}

int PlatformCanvasMac::save(SkCanvas::SaveFlags flags) {
  int ret = SkCanvas::save(flags);
  PlatformDeviceMac& device = getTopPlatformDevice();
  device.SetTransform(getTotalMatrix());
  device.SetClipRegion(getTotalClip());
  return ret;
}

void PlatformCanvasMac::restore() {
  SkCanvas::restore();
  PlatformDeviceMac& device = getTopPlatformDevice();
  device.SetTransform(getTotalMatrix());
  device.SetClipRegion(getTotalClip());
}

SkDevice* PlatformCanvasMac::createDevice(SkBitmap::Config config,
                                       int width,
                                       int height,
                                       bool is_opaque) {
  DCHECK(config == SkBitmap::kARGB_8888_Config);
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

}  // namespace gfx
