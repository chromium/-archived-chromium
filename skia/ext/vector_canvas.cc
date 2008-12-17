// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/vector_canvas.h"

#include "base/logging.h"
#include "skia/ext/vector_device.h"

namespace skia {

VectorCanvas::VectorCanvas() {
}

VectorCanvas::VectorCanvas(HDC dc, int width, int height) {
  bool initialized = initialize(dc, width, height);
  CHECK(initialized);
}

VectorCanvas::~VectorCanvas() {
}

bool VectorCanvas::initialize(HDC context, int width, int height) {
  SkDevice* device = createPlatformDevice(width, height, true, context);
  if (!device)
    return false;

  setDevice(device);
  device->unref();  // was created with refcount 1, and setDevice also refs
  return true;
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

}  // namespace skia

