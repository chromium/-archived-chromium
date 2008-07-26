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

#ifndef CHROME_COMMON_GFX_COLOR_UTILS_H__
#define CHROME_COMMON_GFX_COLOR_UTILS_H__

#include "SkColor.h"

class SkBitmap;

namespace color_utils {

// Represents set of CIE XYZ tristimulus values.
struct CIE_XYZ {
  double X;
  double Y; // luminance
  double Z;
};

// Represents a L*a*b* color value
struct LabColor {
  int L;
  int a;
  int b;
};

// Note: these transformations assume sRGB as the source color space

// Convert between different color spaces
void SkColorToCIEXYZ(SkColor c, CIE_XYZ* xyz);
void CIEXYZToLabColor(const CIE_XYZ& xyz, LabColor* lab);

SkColor CIEXYZToSkColor(SkAlpha alpha, const CIE_XYZ& xyz);
void LabColorToCIEXYZ(const LabColor& lab, CIE_XYZ* xyz);

void SkColorToLabColor(SkColor c, LabColor* lab);
SkColor LabColorToSkColor(const LabColor& lab, SkAlpha alpha);

// Determine if a given alpha value is nearly completely transparent.
bool IsColorCloseToTransparent(SkAlpha alpha);

// Determine if a color is near grey.
bool IsColorCloseToGrey(int r, int g, int b);

// Gets a color representing a bitmap. The definition of "representing" is the
// average color in the bitmap. The color returned is modified to have the
// specified alpha.
SkColor GetAverageColorOfFavicon(SkBitmap* bitmap, SkAlpha alpha);

// Builds a histogram based on the Y' of the Y'UV representation of
// this image.
void BuildLumaHistogram(SkBitmap* bitmap, int histogram[256]);

// Create a color from a base color and a specific alpha value.
SkColor SetColorAlpha(SkColor c, SkAlpha alpha);

// Gets a Windows system color as a SkColor
SkColor GetSysSkColor(int which);

} // namespace color_utils

#endif  // #ifndef CHROME_COMMON_GFX_COLOR_UTILS_H__
