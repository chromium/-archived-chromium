// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/window_sizer.h"

// How much horizontal and vertical offset there is between newly
// opened windows.
const int WindowSizer::kWindowTilePixels = 22;

namespace {

class DefaultMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  DefaultMonitorInfoProvider() { }

  // Overridden from WindowSizer::MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryMonitorWorkArea() const  {
    // Primary monitor is defined as the monitor with the menubar,
    // which is always at index 0.
    NSScreen* primary = [[NSScreen screens] objectAtIndex:0];
    NSRect frame = [primary frame];
    NSRect visible_frame = [primary visibleFrame];

    // Convert coordinate systems.
    gfx::Rect rect = gfx::Rect(NSRectToCGRect(visible_frame));
    rect.set_y(frame.size.height -
               visible_frame.origin.y - visible_frame.size.height);
    return rect;
  }

  virtual gfx::Rect GetPrimaryMonitorBounds() const  {
    // Primary monitor is defined as the monitor with the menubar,
    // which is always at index 0.
    NSScreen* primary = [[NSScreen screens] objectAtIndex:0];
    return gfx::Rect(NSRectToCGRect([primary frame]));
  }

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const {
    // TODO(rohitrao): Support multiple monitors.
    return GetPrimaryMonitorWorkArea();
  }

  virtual gfx::Point GetBoundsOffsetMatching(
      const gfx::Rect& match_rect) const {
    // TODO(rohitrao): Support multiple monitors.
    return GetPrimaryMonitorWorkArea().origin();
  }

  void UpdateWorkAreas() {
    // TODO(rohitrao): Support multiple monitors.
    work_areas_.clear();
    work_areas_.push_back(GetPrimaryMonitorWorkArea());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultMonitorInfoProvider);
};

}  // namespace

// static
WindowSizer::MonitorInfoProvider*
WindowSizer::CreateDefaultMonitorInfoProvider() {
  return new DefaultMonitorInfoProvider();
}

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  return gfx::Point(0, 0);
}
