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

#include "chrome/browser/views/theme_helpers.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atltheme.h>

#include "base/gfx/bitmap_platform_device.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "base/logging.h"
#include "SkGradientShader.h"

void GetRebarGradientColors(int width, int x1, int x2, SkColor* c1, SkColor* c2) {
  DCHECK(c1 && c2) << "ThemeHelpers::GetRebarGradientColors - c1 or c2 is NULL!";

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
  gfx::BitmapPlatformDevice& device =
      static_cast<gfx::BitmapPlatformDevice&>(canvas.getTopPlatformDevice());
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

