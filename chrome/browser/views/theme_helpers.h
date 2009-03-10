// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_THEME_HELPERS_H__
#define CHROME_BROWSER_VIEWS_THEME_HELPERS_H__

#include <windows.h>

#include "SkColor.h"

// Get the colors at two points on a Rebar background gradient. This is for
// drawing Rebar like backgrounds in Views. The reason not to just use
// DrawThemeBackground is that it only draws horizontally, but by extracting
// the colors at two points on the X axis of a background drawn
// by DrawThemeBackground, we can construct a LinearGradientBrush and draw
// such a gradient in any direction.
//
// The width parameter is the width of horizontal gradient that will be
// created to calculate the two colors. x1 and x2 are the two pixel positions
// along the X axis.
void GetRebarGradientColors(int width, int x1, int x2,
                            SkColor* c1, SkColor* c2);


// Gets the color used to draw dark (inset beveled) lines.
void GetDarkLineColor(SkColor* dark_color);

#endif  // #ifndef CHROME_BROWSER_VIEWS_THEME_HELPERS_H__
