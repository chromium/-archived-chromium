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
