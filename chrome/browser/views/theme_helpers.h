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
void GetRebarGradientColors(int width, int x1, int x2, SkColor* c1, SkColor* c2);


// Gets the color used to draw dark (inset beveled) lines.
void GetDarkLineColor(SkColor* dark_color);

#endif  // #ifndef CHROME_BROWSER_VIEWS_THEME_HELPERS_H__
