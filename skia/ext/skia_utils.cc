// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils.h"
#include "skia/include/SkColorPriv.h"

#include "SkGradientShader.h"

namespace skia {

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

// Helper function for HSLToSKColor.
static inline double calcHue(double temp1, double temp2, double hueVal) {
  if (hueVal < 0.0)
    hueVal++;
  else if (hueVal > 1.0)
    hueVal--;

  if (hueVal * 6.0 < 1.0)
    return temp1 + (temp2 - temp1) * hueVal * 6.0;
  if (hueVal * 2.0 < 1.0)
    return temp2;
  if (hueVal * 3.0 < 2.0)
    return temp1 + (temp2 - temp1) * (2.0 / 3.0 - hueVal) * 6.0;

  return temp1;
}

SkPMColor HSLToSKColor(U8CPU alpha, float hsl[3]) {
  double hue = SkScalarToDouble(hsl[0]);
  double saturation = SkScalarToDouble(hsl[1]);
  double lightness = SkScalarToDouble(hsl[2]);
  double scaleFactor = 256.0;

  // If there's no color, we don't care about hue and can do everything based
  // on brightness.
  if (!saturation) {
    U8CPU lightness;

    if (hsl[2] < 0)
      lightness = 0;
    else if (hsl[2] >= SK_Scalar1)
      lightness = 255;
    else
      lightness = SkScalarToFixed(hsl[2]) >> 8;

    unsigned greyValue = SkAlphaMul(lightness, alpha);
    return SkColorSetARGB(alpha, greyValue, greyValue, greyValue);
  }

  double temp2 = (lightness < 0.5) ?
      lightness * (1.0 + saturation) :
      lightness + saturation - (lightness * saturation);
  double temp1 = 2.0 * lightness - temp2;

  double rh = calcHue(temp1, temp2, hue + 1.0 / 3.0);
  double gh = calcHue(temp1, temp2, hue);
  double bh = calcHue(temp1, temp2, hue - 1.0 / 3.0);

  return SkColorSetARGB(alpha,
      SkAlphaMul(static_cast<int>(rh * scaleFactor), alpha),
      SkAlphaMul(static_cast<int>(gh * scaleFactor), alpha),
      SkAlphaMul(static_cast<int>(bh * scaleFactor), alpha));
}

void SkColorToHSL(SkPMColor c, float hsl[3]) {
  double r = SkColorGetR(c) / 255.0;
  double g = SkColorGetG(c) / 255.0;
  double b = SkColorGetB(c) / 255.0;

  double h, s, l;

  double vmax = r > g ? r : g;
  vmax = vmax > b ? vmax : b;
  double vmin = r < g ? r : g;
  vmin = vmin < b ? vmin : b;
  double delta = vmax - vmin;

  l = (vmax + vmin) / 2;

  if (delta == 0) {
    h = 0;
    s = 0;
  } else {
    if (l < 0.5)
      s = delta / (vmax + vmin);
    else
      s = delta / (2 - vmax - vmin);

    double dr = (((vmax - r) / 6.0) + (delta / 2.0)) / delta;
    double dg = (((vmax - g) / 6.0) + (delta / 2.0)) / delta;
    double db = (((vmax - b) / 6.0) + (delta / 2.0)) / delta;

    if (r == vmax)
      h = db - dg;
    else if (g == vmax)
      h = (1.0 / 3.0) + dr - db;
    else if (b == vmax)
      h = (2.0 / 3.0) + dg - dr;

    if (h < 0) h += 1;
    if (h > 1) h -= 1;
  }

  hsl[0] = h;
  hsl[1] = s;
  hsl[2] = l;
}


}  // namespace skia

