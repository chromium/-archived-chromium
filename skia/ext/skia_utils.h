// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_H_
#define SKIA_EXT_SKIA_UTILS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"

namespace skia {

struct HSL {
  double h;
  double s;
  double l;
};

// Creates a vertical gradient shader. The caller owns the shader.
// Example usage to avoid leaks:
//   paint.setShader(gfx::CreateGradientShader(0, 10, red, blue))->safeUnref();
//
// (The old shader in the paint, if any, needs to be freed, and safeUnref will
// handle the NULL case.)
SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color);

// Convert a premultiplied SkColor to a HSL value.
void SkColorToHSL(SkPMColor c, HSL& hsl);

// Convert a HSL color to a premultiplied SkColor.
SkPMColor HSLToSKColor(U8CPU alpha, HSL hsl);

// Shift an HSL value. The shift values are in the range of 0-1,
// with the option to specify -1 for 'no change'. The shift values are
// defined as:
// hsl_shift[0] (hue): The absolute hue value - 0 and 1 map
//    to 0 and 360 on the hue color wheel (red).
// hsl_shift[1] (saturation): A saturation shift, with the
//    following key values:
//    0 = remove all color.
//    0.5 = leave unchanged.
//    1 = fully saturate the image.
// hsl_shift[2] (lightness): A lightness shift, with the
//    following key values:
//    0 = remove all lightness (make all pixels black).
//    0.5 = leave unchanged.
//    1 = full lightness (make all pixels white).
SkColor HSLShift(skia::HSL hsl, skia::HSL shift);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_H_

