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

#include "base/gfx/vector_canvas.h"

#include "base/gfx/vector_device.h"
#include "base/logging.h"

namespace gfx {

VectorCanvas::VectorCanvas() {
}

VectorCanvas::VectorCanvas(HDC dc, int width, int height) {
  initialize(dc, width, height);
}

VectorCanvas::~VectorCanvas() {
}

void VectorCanvas::initialize(HDC context, int width, int height) {
  SkDevice* device = createPlatformDevice(width, height, true, context);
  setDevice(device);
  device->unref();  // was created with refcount 1, and setDevice also refs
}

SkBounder* VectorCanvas::setBounder(SkBounder* bounder) {
  if (!IsTopDeviceVectorial())
    return PlatformCanvasWin::setBounder(bounder);

  // This function isn't used in the code. Verify this assumption.
  NOTREACHED();
  return NULL;
}

SkDevice* VectorCanvas::createDevice(SkBitmap::Config config,
                                     int width, int height,
                                     bool is_opaque, bool isForLayer) {
  DCHECK(config == SkBitmap::kARGB_8888_Config);
  return createPlatformDevice(width, height, is_opaque, NULL);
}

SkDrawFilter* VectorCanvas::setDrawFilter(SkDrawFilter* filter) {
  // This function isn't used in the code. Verify this assumption.
  NOTREACHED();
  return NULL;
}

SkDevice* VectorCanvas::createPlatformDevice(int width,
                                             int height, bool is_opaque,
                                             HANDLE shared_section) {
  if (!is_opaque) {
    // TODO(maruel):  http://b/1184002 1184002 When restoring a semi-transparent
    // layer, i.e. merging it, we need to rasterize it because GDI doesn't
    // support transparency except for AlphaBlend(). Right now, a
    // BitmapPlatformDeviceWin is created when VectorCanvas think a saveLayers()
    // call is being done. The way to save a layer would be to create an
    // EMF-based VectorDevice and have this device registers the drawing. When
    // playing back the device into a bitmap, do it at the printer's dpi instead
    // of the layout's dpi (which is much lower).
    return PlatformCanvasWin::createPlatformDevice(width, height, is_opaque,
                                                shared_section);
  }

  // TODO(maruel):  http://b/1183870 Look if it would be worth to increase the
  // resolution by ~10x (any worthy factor) to increase the rendering precision
  // (think about printing) while using a relatively low dpi. This happens
  // because we receive float as input but the GDI functions works with
  // integers. The idea is to premultiply the matrix with this factor and
  // multiply each SkScalar that are passed to SkScalarRound(value) as
  // SkScalarRound(value * 10). Safari is already doing the same for text
  // rendering.
  DCHECK(shared_section);
  PlatformDeviceWin* device = VectorDevice::create(
      reinterpret_cast<HDC>(shared_section), width, height);
  return device;
}

bool VectorCanvas::IsTopDeviceVectorial() const {
  return getTopPlatformDevice().IsVectorial();
}

}  // namespace gfx
