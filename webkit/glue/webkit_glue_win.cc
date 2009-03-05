// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include "webkit/glue/screen_info.h"

namespace webkit_glue {

ScreenInfo GetScreenInfoHelper(gfx::NativeView window) {
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

  MONITORINFOEX monitor_info;
  monitor_info.cbSize = sizeof(MONITORINFOEX);
  GetMonitorInfo(monitor, &monitor_info);

  DEVMODE dev_mode;
  dev_mode.dmSize = sizeof(dev_mode);
  dev_mode.dmDriverExtra = 0;
  EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &dev_mode);

  ScreenInfo results;
  results.depth = dev_mode.dmBitsPerPel;
  results.depth_per_component = dev_mode.dmBitsPerPel / 3;  // Assumes RGB
  results.is_monochrome = dev_mode.dmColor == DMCOLOR_MONOCHROME;
  results.rect = gfx::Rect(monitor_info.rcMonitor);
  results.available_rect = gfx::Rect(monitor_info.rcWork);
  return results;
}

}  // namespace webkit_glue
