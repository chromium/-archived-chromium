// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_WIN_H_
#define SKIA_EXT_CANVAS_PAINT_WIN_H_

#include "skia/ext/platform_canvas.h"

namespace skia {

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
//     gfx::PlatformCanvasPaint canvas(hwnd);
//     if (!canvas.isEmpty()) {
//       ... paint to the canvas ...
//     }
//     return 0;
//   }
template <class T>
class CanvasPaintT : public T {
 public:
  // This constructor assumes the canvas is opaque.
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

typedef CanvasPaintT<PlatformCanvas> PlatformCanvasPaint;

}  // namespace skia

#endif  // SKIA_EXT_CANVAS_PAINT_WIN_H_
