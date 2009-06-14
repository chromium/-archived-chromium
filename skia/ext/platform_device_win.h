// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_DEVICE_WIN_H_
#define SKIA_EXT_PLATFORM_DEVICE_WIN_H_

#include <windows.h>

#include <vector>

#include "SkDevice.h"

class SkMatrix;
class SkPath;
class SkRegion;

namespace skia {

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. Our device provides a surface Windows can also write
// to. It also provides functionality to play well with GDI drawing functions.
// This class is abstract and must be subclassed. It provides the basic
// interface to implement it either with or without a bitmap backend.
class PlatformDevice : public SkDevice {
 public:
  typedef HDC PlatformSurface;

  // The DC that corresponds to the bitmap, used for GDI operations drawing
  // into the bitmap. This is possibly heavyweight, so it should be existant
  // only during one pass of rendering.
  virtual HDC getBitmapDC() = 0;

  // Draws to the given screen DC, if the bitmap DC doesn't exist, this will
  // temporarily create it. However, if you have created the bitmap DC, it will
  // be more efficient if you don't free it until after this call so it doesn't
  // have to be created twice.  If src_rect is null, then the entirety of the
  // source device will be copied.
  virtual void drawToHDC(HDC dc, int x, int y, const RECT* src_rect) = 0;

  // Sets the opacity of each pixel in the specified region to be opaque.
  virtual void makeOpaque(int x, int y, int width, int height) { }

  // Returns if the preferred rendering engine is vectorial or bitmap based.
  virtual bool IsVectorial() = 0;

  // Initializes the default settings and colors in a device context.
  static void InitializeDC(HDC context);

  // Loads a SkPath into the GDI context. The path can there after be used for
  // clipping or as a stroke.
  static void LoadPathToDC(HDC context, const SkPath& path);

  // Loads a SkRegion into the GDI context.
  static void LoadClippingRegionToDC(HDC context, const SkRegion& region,
                                     const SkMatrix& transformation);

 protected:
  // Arrays must be inside structures.
  struct CubicPoints {
    SkPoint p[4];
  };
  typedef std::vector<CubicPoints> CubicPath;
  typedef std::vector<CubicPath> CubicPaths;

  // Forwards |bitmap| to SkDevice's constructor.
  PlatformDevice(const SkBitmap& bitmap);

  // Loads the specified Skia transform into the device context, excluding
  // perspective (which GDI doesn't support).
  static void LoadTransformToDC(HDC dc, const SkMatrix& matrix);

  // Transforms SkPath's paths into a series of cubic path.
  static bool SkPathToCubicPaths(CubicPaths* paths, const SkPath& skpath);
};

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_DEVICE_WIN_H_

