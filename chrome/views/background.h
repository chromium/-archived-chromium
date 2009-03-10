// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_BACKGROUND_H__
#define CHROME_VIEWS_BACKGROUND_H__

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

#include "base/basictypes.h"
#include "SkColor.h"

class ChromeCanvas;

namespace views {

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

#if defined(OS_WIN)
  // TODO(port): Make GetNativeControlBrush portable (currently uses HBRUSH).

  // Get the brush that was specified by SetNativeControlColor
  HBRUSH GetNativeControlBrush() const { return native_control_brush_; };
#endif  // defined(OS_WIN)

 private:
#if defined(OS_WIN)
  // TODO(port): Create portable replacement for HBRUSH.
  HBRUSH native_control_brush_;
#endif  // defined(OS_WIN)
  DISALLOW_COPY_AND_ASSIGN(Background);
};

}  // namespace views

#endif  // CHROME_VIEWS_BACKGROUND_H__
