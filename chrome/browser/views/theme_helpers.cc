// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/theme_helpers.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atltheme.h>

#include "chrome/common/gfx/chrome_canvas.h"
#include "base/logging.h"
#include "skia/ext/bitmap_platform_device_win.h"
#include "SkGradientShader.h"

void GetRebarGradientColors(int width, int x1, int x2,
                            SkColor* c1, SkColor* c2) {
  DCHECK(c1 && c2) <<
      "ThemeHelpers::GetRebarGradientColors - c1 or c2 is NULL!";

  // To get the colors we need, we draw a horizontal gradient using
  // DrawThemeBackground, then extract the pixel values from and return
  // those so calling code can use them to create gradient brushes for use in
  // rendering in other directions.

  ChromeCanvas canvas(width, 1, true);

  // Render the Rebar gradient into the DIB
  CTheme theme;
  if (theme.IsThemingSupported())
    theme.OpenThemeData(NULL, L"REBAR");
  // On Windows XP+, if using a Theme, we can ask the theme to render the
  // gradient for us.
  if (!theme.IsThemeNull()) {
    HDC dc = canvas.beginPlatformPaint();
    RECT rect = { 0, 0, width, 1 };
    theme.DrawThemeBackground(dc, 0, 0, &rect, NULL);
    canvas.endPlatformPaint();
  } else {
    // On Windows 2000 or Windows XP+ with the Classic theme selected, we need
    // to build our own gradient using system colors.
    SkColor grad_colors[2];
    COLORREF hl_ref = ::GetSysColor(COLOR_3DHILIGHT);
    grad_colors[0] = SkColorSetRGB(GetRValue(hl_ref), GetGValue(hl_ref),
                                   GetBValue(hl_ref));
    COLORREF face_ref = ::GetSysColor(COLOR_3DFACE);
    grad_colors[1] = SkColorSetRGB(GetRValue(face_ref), GetGValue(face_ref),
                                   GetBValue(face_ref));
    SkPoint grad_points[2];
    grad_points[0].set(SkIntToScalar(0), SkIntToScalar(0));
    grad_points[1].set(SkIntToScalar(width), SkIntToScalar(0));
    SkShader* gradient_shader = SkGradientShader::CreateLinear(
        grad_points, grad_colors, NULL, 2, SkShader::kRepeat_TileMode);
    SkPaint paint;
    paint.setShader(gradient_shader);
    // Shader created with a ref count of 1, release as the paint now owns
    // the gradient.
    gradient_shader->unref();
    paint.setStyle(SkPaint::kFill_Style);
    canvas.drawRectCoords(SkIntToScalar(0), SkIntToScalar(0),
                          SkIntToScalar(width), SkIntToScalar(1), paint);
  }

  // Extract the color values from the selected pixels
  // The | in the following operations forces the alpha to 0xFF. This is
  // needed as windows sets the alpha to 0 when it renders.
  skia::BitmapPlatformDeviceWin& device =
      static_cast<skia::BitmapPlatformDeviceWin&>(
          canvas.getTopPlatformDevice());
  *c1 = 0xFF000000 | device.getColorAt(x1, 0);
  *c2 = 0xFF000000 | device.getColorAt(x2, 0);
}

void GetDarkLineColor(SkColor* dark_color) {
  DCHECK(dark_color) << "ThemeHelpers::DarkColor - dark_color is NULL!";

  CTheme theme;
  if (theme.IsThemingSupported())
    theme.OpenThemeData(NULL, L"REBAR");

  // Note: the alpha values were chosen scientifically according to what looked
  //       best to me at the time! --beng
  if (!theme.IsThemeNull()) {
    *dark_color = SkColorSetARGB(60, 0, 0, 0);
  } else {
    COLORREF shadow_ref = ::GetSysColor(COLOR_3DSHADOW);
    *dark_color = SkColorSetARGB(175,
                                 GetRValue(shadow_ref),
                                 GetGValue(shadow_ref),
                                 GetBValue(shadow_ref));
  }
}


