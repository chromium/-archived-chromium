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

#include "base/gfx/skia_utils.h"

#include "base/logging.h"
#include "SkRect.h"
#include "SkGradientShader.h"

namespace {

COMPILE_ASSERT(offsetof(RECT, left) == offsetof(SkIRect, fLeft), o1);
COMPILE_ASSERT(offsetof(RECT, top) == offsetof(SkIRect, fTop), o2);
COMPILE_ASSERT(offsetof(RECT, right) == offsetof(SkIRect, fRight), o3);
COMPILE_ASSERT(offsetof(RECT, bottom) == offsetof(SkIRect, fBottom), o4);
COMPILE_ASSERT(sizeof(RECT().left) == sizeof(SkIRect().fLeft), o5);
COMPILE_ASSERT(sizeof(RECT().top) == sizeof(SkIRect().fTop), o6);
COMPILE_ASSERT(sizeof(RECT().right) == sizeof(SkIRect().fRight), o7);
COMPILE_ASSERT(sizeof(RECT().bottom) == sizeof(SkIRect().fBottom), o8);
COMPILE_ASSERT(sizeof(RECT) == sizeof(SkIRect), o9);

}  // namespace

namespace gfx {

POINT SkPointToPOINT(const SkPoint& point) {
  POINT win_point = { SkScalarRound(point.fX), SkScalarRound(point.fY) };
  return win_point;
}

SkRect RECTToSkRect(const RECT& rect) {
  SkRect sk_rect = { SkIntToScalar(rect.left), SkIntToScalar(rect.top),
                     SkIntToScalar(rect.right), SkIntToScalar(rect.bottom) };
  return sk_rect;
}

SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color) {
  SkColor grad_colors[2] = { start_color, end_color};
  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(start_point));
  grad_points[1].set(SkIntToScalar(0), SkIntToScalar(end_point));

  return SkGradientShader::CreateLinear(
      grad_points, grad_colors, NULL, 2, SkShader::kRepeat_TileMode);
}


SkColor COLORREFToSkColor(COLORREF color) {
#ifndef _MSC_VER
  return SkColorSetRGB(GetRValue(color), GetGValue(color), GetBValue(color));
#else
  // ARGB = 0xFF000000 | ((0BGR -> RGB0) >> 8)
  return 0xFF000000u | (_byteswap_ulong(color) >> 8);
#endif
}

COLORREF SkColorToCOLORREF(SkColor color) {
  // Currently, Alpha is always 255 or the color is 0 so there is no need to
  // demultiply the channels. If this DCHECK() is ever hit, the full
  // (SkColorGetX(color) * 255 / a) will have to be added in the conversion.
  DCHECK((0xFF == SkColorGetA(color)) || (0 == color));
#ifndef _MSC_VER
  return RGB(SkColorGetR(color), SkColorGetG(color), SkColorGetB(color));
#else
  // 0BGR = ((ARGB -> BGRA) >> 8)
  return (_byteswap_ulong(color) >> 8);
#endif
}

}  // namespace gfx
