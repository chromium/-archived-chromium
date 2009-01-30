// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_CANVAS_MAC_H_
#define SKIA_EXT_PLATFORM_CANVAS_MAC_H_

#include "skia/ext/platform_device_mac.h"

#include "SkCanvas.h"

namespace skia {

// This class is a specialization of the regular SkCanvas that is designed to
// work with a gfx::PlatformDevice to manage platform-specific drawing. It
// allows using both Skia operations and platform-specific operations.
class PlatformCanvasMac : public SkCanvas {
 public:
  // Set is_opaque if you are going to erase the bitmap and not use
  // tranparency: this will enable some optimizations.  The shared_section
  // parameter is passed to gfx::PlatformDevice::create.  See it for details.
  //
  // If you use the version with no arguments, you MUST call initialize()
  PlatformCanvasMac();
  PlatformCanvasMac(int width, int height, bool is_opaque);
  PlatformCanvasMac(int width, int height, bool is_opaque,
                    CGContextRef context);
  virtual ~PlatformCanvasMac();

  // For two-part init, call if you use the no-argument constructor above
  bool initialize(int width, int height, bool is_opaque);

  // These calls should surround calls to platform drawing routines. The CG
  // context returned by beginPlatformPaint is the one that can be used to
  // draw into.
  // Call endPlatformPaint when you are done and want to use Skia operations
  // again; this will synchronize the bitmap.
  virtual CGContextRef beginPlatformPaint();
  virtual void endPlatformPaint();

  // Returns the platform device pointer of the topmost rect with a non-empty
  // clip. In practice, this is usually either the top layer or nothing, since
  // we usually set the clip to new layers when we make them.
  //
  // If there is no layer that is not all clipped out, this will return a
  // dummy device so callers do not have to check. If you are concerned about
  // performance, check the clip before doing any painting.
  //
  // This is different than SkCanvas' getDevice, because that returns the
  // bottommost device.
  //
  // Danger: the resulting device should not be saved. It will be invalidated
  // by the next call to save() or restore().
  PlatformDeviceMac& getTopPlatformDevice() const;

  // Allow callers to see the non-virtual function even though we have an
  // override of a virtual one.
  using SkCanvas::clipRect;

 protected:
  // Creates a device store for use by the canvas. We override this so that
  // the device is always our own so we know that we can use GDI operations
  // on it. Simply calls into createPlatformDevice().
  virtual SkDevice* createDevice(SkBitmap::Config, int width, int height,
                                 bool is_opaque, bool isForLayer);

  // Creates a device store for use by the canvas. By default, it creates a
  // BitmapPlatformDevice object. Can be overridden to change the object type.
  virtual SkDevice* createPlatformDevice(int width, int height, bool is_opaque,
                                         CGContextRef context);

 private:
  // Unimplemented. This is to try to prevent people from calling this function
  // on SkCanvas. SkCanvas' version is not virtual, so we can't prevent this
  // 100%, but hopefully this will make people notice and not use the function.
  // Calling SkCanvas' version will create a new device which is not compatible
  // with us and we will crash if somebody tries to draw into it with
  // CoreGraphics.
  SkDevice* setBitmapDevice(const SkBitmap& bitmap);

  // Disallow copy and assign.
  PlatformCanvasMac(const PlatformCanvasMac&);
  PlatformCanvasMac& operator=(const PlatformCanvasMac&);
};

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_CANVAS_MAC_H_
