// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

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

SkPMColor HSLToSKColor(U8CPU alpha, HSL hsl) {
  double hue = hsl.h;
  double saturation = hsl.s;
  double lightness = hsl.l;
  double scaleFactor = 256.0;

  // If there's no color, we don't care about hue and can do everything based
  // on brightness.
  if (!saturation) {
    U8CPU light;

    if (lightness < 0)
      light = 0;
    else if (lightness >= SK_Scalar1)
      light = 255;
    else
      light = SkDoubleToFixed(lightness) >> 8;

    unsigned greyValue = SkAlphaMul(light, alpha);
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

void SkColorToHSL(SkPMColor c, HSL& hsl) {
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

  hsl.h = h;
  hsl.s = s;
  hsl.l = l;
}

SkColor HSLShift(HSL hsl, HSL shift) {
  // Replace the hue with the tint's hue.
  if (shift.h >= 0)
    hsl.h = shift.h;

  // Change the saturation.
  if (shift.s >= 0) {
    if (shift.s <= 0.5) {
      hsl.s *= shift.s * 2.0;
    } else {
      hsl.s = hsl.s + (1.0 - hsl.s) *
        ((shift.s - 0.5) * 2.0);
    }
  }

  // Change the lightness.
  if (shift.l >= 0) {
    if (shift.l <= 0.5) {
      hsl.l *= shift.l * 2.0;
    } else {
      hsl.l = hsl.l + (1.0 - hsl.l) *
        ((shift.l - 0.5) * 2.0);
    }
  }

  return skia::HSLToSKColor(0xff, hsl);
}


}  // namespace skia

