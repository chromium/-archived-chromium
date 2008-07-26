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

#ifndef CHROME_VIEWS_BACKGROUND_H__
#define CHROME_VIEWS_BACKGROUND_H__

#include <windows.h>

#include "base/basictypes.h"
#include "SkColor.h"

class ChromeCanvas;

namespace ChromeViews {

class Painter;
class View;

/////////////////////////////////////////////////////////////////////////////
//
// Background class
//
// A background implements a way for views to paint a background. The
// background can be either solid or based on a gradient. Of course,
// Background can be subclassed to implement various effects.
//
// Any View can have a background. See View::SetBackground() and
// View::PaintBackground()
//
/////////////////////////////////////////////////////////////////////////////
class Background {
 public:
  Background();
  virtual ~Background();

  // Creates a background that fills the canvas in the specified color.
  static Background* CreateSolidBackground(const SkColor& color);

  // Creates a background that fills the canvas in the specified color.
  static Background* CreateSolidBackground(int r, int g, int b) {
    return CreateSolidBackground(SkColorSetRGB(r, g, b));
  }

  // Creates a background that fills the canvas in the specified color.
  static Background* CreateSolidBackground(int r, int g, int b, int a) {
    return CreateSolidBackground(SkColorSetARGB(a, r, g, b));
  }

  // Creates a background that contains a vertical gradient that varies
  // from |color1| to |color2|
  static Background* CreateVerticalGradientBackground(const SkColor& color1,
                                                      const SkColor& color2);

  // Creates Chrome's standard panel background
  static Background* CreateStandardPanelBackground();

  // Creates a Background from the specified Painter. If owns_painter is
  // true, the Painter is deleted when the Border is deleted.
  static Background* CreateBackgroundPainter(bool owns_painter,
                                             Painter* painter);

  // Render the background for the provided view
  virtual void Paint(ChromeCanvas* canvas, View* view) const = 0;

  // Set a solid, opaque color to be used when drawing backgrounds of native
  // controls.  Unfortunately alpha=0 is not an option.
  void SetNativeControlColor(SkColor color);

  // Get the brush that was specified by SetNativeControlColor
  HBRUSH GetNativeControlBrush() const { return native_control_brush_; };

 private:
  HBRUSH native_control_brush_;
  DISALLOW_EVIL_CONSTRUCTORS(Background);
};

}
#endif  // CHROME_VIEWS_BACKGROUND_H__
