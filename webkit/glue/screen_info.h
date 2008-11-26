// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_SCREEN_INFO_H_
#define WEBKIT_GLUE_SCREEN_INFO_H_

#include "base/gfx/rect.h"

namespace webkit_glue {

struct ScreenInfo {
  // The screen depth in bits per pixel
  int depth;
  // The bits per colour component. This assumes that the colours are balanced
  // equally.
  int depth_per_component;
  // This can be true for black and white printers
  bool is_monochrome;
  // This is set from the rcMonitor member of MONITORINFOEX, to whit:
  //   "A RECT structure that specifies the display monitor rectangle,
  //   expressed in virtual-screen coordinates. Note that if the monitor is not
  //   the primary display monitor, some of the rectangle's coordinates may be
  //   negative values."
  gfx::Rect rect;
  // This is set from the rcWork member of MONITORINFOEX, to whit:
  //   "A RECT structure that specifies the work area rectangle of the display
  //   monitor that can be used by applications, expressed in virtual-screen
  //   coordinates. Windows uses this rectangle to maximize an application on
  //   the monitor. The rest of the area in rcMonitor contains system windows
  //   such as the task bar and side bars. Note that if the monitor is not the
  //   primary display monitor, some of the rectangle's coordinates may be
  //   negative values".
  gfx::Rect available_rect;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_SCREEN_INFO_H_
