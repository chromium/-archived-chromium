// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/platform_device_mac.h"

#include "base/logging.h"
#include "base/gfx/skia_utils_mac.h"
#include "SkMatrix.h"
#include "SkPath.h"
#include "SkUtils.h"

namespace gfx {

namespace {

// Constrains position and size to fit within available_size.
bool constrain(int available_size, int* position, int *size) {
  if (*position < 0) {
    *size += *position;
    *position = 0;
  }
  if (*size > 0 && *position < available_size) {
    int overflow = (*position + *size) - available_size;
    if (overflow > 0) {
      *size -= overflow;
    }
    return true;
  }
  return false;
}

// Sets the opacity of the specified value to 0xFF.
void makeOpaqueAlphaAdjuster(uint32_t* pixel) {
  *pixel |= 0xFF000000;
}

} // namespace

PlatformDeviceMac::PlatformDeviceMac(const SkBitmap& bitmap)
    : SkDevice(bitmap) {
}

void PlatformDeviceMac::makeOpaque(int x, int y, int width, int height) {
  processPixels(x, y, width, height, makeOpaqueAlphaAdjuster);
}

// Set up the CGContextRef for peaceful coexistence with Skia
void PlatformDeviceMac::InitializeCGContext(CGContextRef context) {
  // CG defaults to the same settings as Skia
}

// static
void PlatformDeviceMac::LoadPathToCGContext(CGContextRef context,
                                            const SkPath& path) {
  // instead of a persistent attribute of the context, CG specifies the fill
  // type per call, so we just have to load up the geometry.
  CGContextBeginPath(context);

  SkPoint points[4] = {0};
  SkPath::Iter iter(path, false);
  for (SkPath::Verb verb = iter.next(points); verb != SkPath::kDone_Verb;
       verb = iter.next(points)) {
    switch (verb) {
      case SkPath::kMove_Verb: {  // iter.next returns 1 point
        CGContextMoveToPoint(context, points[0].fX, points[0].fY);
        break;
      }
      case SkPath::kLine_Verb: {  // iter.next returns 2 points
        CGContextAddLineToPoint(context, points[1].fX, points[1].fY);
        break;
      }
      case SkPath::kQuad_Verb: {  // iter.next returns 3 points
        CGContextAddQuadCurveToPoint(context, points[1].fX, points[1].fY,
                                     points[2].fX, points[2].fY);
        break;
      }
      case SkPath::kCubic_Verb: {  // iter.next returns 4 points
        CGContextAddCurveToPoint(context, points[1].fX, points[1].fY,
                                 points[2].fX, points[2].fY,
                                 points[3].fX, points[3].fY);
        break;
      }
      case SkPath::kClose_Verb: {  // iter.next returns 1 point (the last point)
        break;
      }
      case SkPath::kDone_Verb:  // iter.next returns 0 points
      default: {
        NOTREACHED();
        break;
      }
    }
  }
  CGContextClosePath(context);
}

// static
void PlatformDeviceMac::LoadTransformToCGContext(CGContextRef context,
                                                 const SkMatrix& matrix) {
  // TOOD: CoreGraphics can concatenate transforms, but not reset the current
  // ont.  Either find a workaround or remove this function if it turns out
  // to be unneeded on the Mac.
}

}  // namespace gfx

