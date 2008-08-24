// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_PLATFORM_CANVAS_MAC_H__
#define BASE_GFX_PLATFORM_CANVAS_MAC_H__

#include "base/gfx/platform_device_mac.h"
#include "base/basictypes.h"

#import "SkCanvas.h"

namespace gfx {

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
  PlatformCanvasMac(int width, int height, bool is_opaque, CGContextRef context);
  virtual ~PlatformCanvasMac();

  // For two-part init, call if you use the no-argument constructor above
  void initialize(int width, int height, bool is_opaque, CGContextRef context);

  // Keep the platform clipping in sync with the skia clipping. Note that
  // platform clipping may only clip to the bounds of the clipping region, if
  // it is complex.
  virtual bool clipRect(const SkRect& rect,
                        SkRegion::Op op = SkRegion::kIntersect_Op);
  virtual bool clipPath(const SkPath& path,
                        SkRegion::Op op = SkRegion::kIntersect_Op);
  virtual bool clipRegion(const SkRegion& deviceRgn,
                          SkRegion::Op op = SkRegion::kIntersect_Op);

  // These calls should surround calls to platform drawing routines. The CG
  // context returned by beginPlatformPaint is the one that can be used to
  // draw into.
  // Call endPlatformPaint when you are done and want to use Skia operations
  // again; this will synchronize the bitmap.
  virtual CGContextRef beginPlatformPaint();
  virtual void endPlatformPaint();

  // overridden to keep the platform graphics context in sync with the canvas
  virtual bool translate(SkScalar dx, SkScalar dy);
  virtual bool scale(SkScalar sx, SkScalar sy);
  virtual int saveLayer(const SkRect* bounds, const SkPaint* paint,
                        SaveFlags flags = kARGB_ClipLayer_SaveFlag);
  virtual int save(SkCanvas::SaveFlags flags = SkCanvas::kMatrixClip_SaveFlag);
  virtual void restore();

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
                                 bool is_opaque);

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

  DISALLOW_COPY_AND_ASSIGN(PlatformCanvasMac);
};

}  // namespace gfx

#endif  // BASE_GFX_PLATFORM_CANVAS_MAC_H__

