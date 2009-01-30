// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_CANVAS_WIN_H_
#define SKIA_EXT_PLATFORM_CANVAS_WIN_H_

#include <windows.h>

#include "skia/ext/platform_device_win.h"

#include "SkCanvas.h"

namespace skia {

// This class is a specialization of the regular SkCanvas that is designed to
// work with a PlatformDevice to manage platform-specific drawing. It allows
// using both Skia operations and platform-specific operations.
class PlatformCanvasWin : public SkCanvas {
 public:
  // Set is_opaque if you are going to erase the bitmap and not use
  // transparency: this will enable some optimizations.  The shared_section
  // parameter is passed to gfx::PlatformDevice::create.  See it for details.
  //
  // If you use the version with no arguments, you MUST call initialize()
  PlatformCanvasWin();
  PlatformCanvasWin(int width, int height, bool is_opaque);
  PlatformCanvasWin(int width, int height, bool is_opaque,
                    HANDLE shared_section);
  virtual ~PlatformCanvasWin();

  // For two-part init, call if you use the no-argument constructor above. Note
  // that we want this to optionally match the Linux initialize if you only
  // pass 3 arguments, hence the evil default argument.
  bool initialize(int width, int height, bool is_opaque,
                  HANDLE shared_section = NULL);

  // These calls should surround calls to platform drawing routines, the DC
  // returned by beginPlatformPaint is the DC that can be used to draw into.
  // Call endPlatformPaint when you are done and want to use Skia operations
  // again; this will synchronize the bitmap to Windows.
  virtual HDC beginPlatformPaint();
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
  PlatformDeviceWin& getTopPlatformDevice() const;

 protected:
  // Creates a device store for use by the canvas. We override this so that
  // the device is always our own so we know that we can use GDI operations
  // on it. Simply calls into createPlatformDevice().
  virtual SkDevice* createDevice(SkBitmap::Config, int width, int height,
                                 bool is_opaque, bool isForLayer);

  // Creates a device store for use by the canvas. By default, it creates a
  // BitmapPlatformDeviceWin. Can be overridden to change the object type.
  virtual SkDevice* createPlatformDevice(int width, int height, bool is_opaque,
                                         HANDLE shared_section);

 private:
  // Unimplemented.
  virtual SkDevice* setBitmapDevice(const SkBitmap& bitmap);

  // Disallow copy and assign.
  PlatformCanvasWin(const PlatformCanvasWin&);
  PlatformCanvasWin& operator=(const PlatformCanvasWin&);
};

// A class designed to help with WM_PAINT operations on Windows. It will
// do BeginPaint/EndPaint on init/destruction, and will create the bitmap and
// canvas with the correct size and transform for the dirty rect. The bitmap
// will be automatically painted to the screen on destruction.
//
// You MUST call isEmpty before painting to determine if anything needs
// painting. Sometimes the dirty rect can actually be empty, and this makes
// the bitmap functions we call unhappy. The caller should not paint in this
// case.
//
// Therefore, all you need to do is:
//   case WM_PAINT: {
//     gfx::PlatformCanvasWinPaint canvas(hwnd);
//     if (!canvas.isEmpty()) {
//       ... paint to the canvas ...
//     }
//     return 0;
//   }
template <class T>
class CanvasPaintT : public T {
 public:
  explicit CanvasPaintT(HWND hwnd) : hwnd_(hwnd), paint_dc_(NULL),
    for_paint_(true) {
    memset(&ps_, 0, sizeof(ps_));
    initPaint(true);
  }

  CanvasPaintT(HWND hwnd, bool opaque) : hwnd_(hwnd), paint_dc_(NULL),
      for_paint_(true) {
    memset(&ps_, 0, sizeof(ps_));
    initPaint(opaque);
  }

  // Creates a CanvasPaintT for the specified region that paints to the
  // specified dc. This does NOT do BeginPaint/EndPaint.
  CanvasPaintT(HDC dc, bool opaque, int x, int y, int w, int h)
      : hwnd_(NULL),
        paint_dc_(dc),
        for_paint_(false) {
    memset(&ps_, 0, sizeof(ps_));
    ps_.rcPaint.left = x;
    ps_.rcPaint.right = x + w;
    ps_.rcPaint.top = y;
    ps_.rcPaint.bottom = y + h;
    init(opaque);
  }


  virtual ~CanvasPaintT() {
    if (!isEmpty()) {
      restoreToCount(1);
      // Commit the drawing to the screen
      getTopPlatformDevice().drawToHDC(paint_dc_,
                                       ps_.rcPaint.left, ps_.rcPaint.top,
                                       NULL);
    }
    if (for_paint_)
      EndPaint(hwnd_, &ps_);
  }

  // Returns true if the invalid region is empty. The caller should call this
  // function to determine if anything needs painting.
  bool isEmpty() const {
    return ps_.rcPaint.right - ps_.rcPaint.left == 0 ||
           ps_.rcPaint.bottom - ps_.rcPaint.top == 0;
  }

  // Use to access the Windows painting parameters, especially useful for
  // getting the bounding rect for painting: paintstruct().rcPaint
  const PAINTSTRUCT& paintStruct() const {
    return ps_;
  }

  // Returns the DC that will be painted to
  HDC paintDC() const {
    return paint_dc_;
  }

 protected:
  HWND hwnd_;
  HDC paint_dc_;
  PAINTSTRUCT ps_;

 private:
  void initPaint(bool opaque) {
    paint_dc_ = BeginPaint(hwnd_, &ps_);

    init(opaque);
  }

  void init(bool opaque) {
    // FIXME(brettw) for ClearType, we probably want to expand the bounds of
    // painting by one pixel so that the boundaries will be correct (ClearType
    // text can depend on the adjacent pixel). Then we would paint just the
    // inset pixels to the screen.
    const int width = ps_.rcPaint.right - ps_.rcPaint.left;
    const int height = ps_.rcPaint.bottom - ps_.rcPaint.top;
    if (!initialize(width, height, opaque, NULL)) {
      // Cause a deliberate crash;
      *(char*) 0 = 0;
    }

    // This will bring the canvas into the screen coordinate system for the
    // dirty rect
    translate(SkIntToScalar(-ps_.rcPaint.left),
              SkIntToScalar(-ps_.rcPaint.top));
  }

  // If true, this canvas was created for a BeginPaint.
  const bool for_paint_;

  // Disallow copy and assign.
  CanvasPaintT(const CanvasPaintT&);
  CanvasPaintT& operator=(const CanvasPaintT&);
};

typedef CanvasPaintT<PlatformCanvasWin> PlatformCanvasWinPaint;

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_CANVAS_WIN_H_
