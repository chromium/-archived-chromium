// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/window_sizer.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"

// How much horizontal and vertical offset there is between newly
// opened windows.
const int WindowSizer::kWindowTilePixels = 10;

// An implementation of WindowSizer::MonitorInfoProvider that gets the actual
// monitor information from Windows.
class DefaultMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  DefaultMonitorInfoProvider() { }

  // Overridden from WindowSizer::MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryMonitorWorkArea() const {
    return gfx::Rect(GetMonitorInfoForMonitor(MonitorFromWindow(NULL,
        MONITOR_DEFAULTTOPRIMARY)).rcWork);
  }

  virtual gfx::Rect GetPrimaryMonitorBounds() const {
    return gfx::Rect(GetMonitorInfoForMonitor(MonitorFromWindow(NULL,
        MONITOR_DEFAULTTOPRIMARY)).rcMonitor);
  }

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const {
    CRect other_bounds_crect = match_rect.ToRECT();
    MONITORINFO monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
        &other_bounds_crect, MONITOR_DEFAULTTONEAREST));
    return gfx::Rect(monitor_info.rcWork);
  }

  virtual gfx::Point GetBoundsOffsetMatching(
      const gfx::Rect& match_rect) const {
    CRect other_bounds_crect = match_rect.ToRECT();
    MONITORINFO monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
        &other_bounds_crect, MONITOR_DEFAULTTONEAREST));
    return gfx::Point(monitor_info.rcWork.left - monitor_info.rcMonitor.left,
                      monitor_info.rcWork.top - monitor_info.rcMonitor.top);
  }

  void UpdateWorkAreas() {
    work_areas_.clear();
    EnumDisplayMonitors(NULL, NULL,
                        &DefaultMonitorInfoProvider::MonitorEnumProc,
                        reinterpret_cast<LPARAM>(&work_areas_));
  }

 private:
  // A callback for EnumDisplayMonitors that records the work area of the
  // current monitor in the enumeration.
  static BOOL CALLBACK MonitorEnumProc(HMONITOR monitor,
                                       HDC monitor_dc,
                                       LPRECT monitor_rect,
                                       LPARAM data) {
    reinterpret_cast<std::vector<gfx::Rect>*>(data)->push_back(
        gfx::Rect(GetMonitorInfoForMonitor(monitor).rcWork));
    return TRUE;
  }

  static MONITORINFO GetMonitorInfoForMonitor(HMONITOR monitor) {
    MONITORINFO monitor_info = { 0 };
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(monitor, &monitor_info);
    return monitor_info;
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultMonitorInfoProvider);
};

// static
WindowSizer::MonitorInfoProvider*
WindowSizer::CreateDefaultMonitorInfoProvider() {
  return new DefaultMonitorInfoProvider();
}

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  RECT area;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &area, 0);
  gfx::Point corner(area.left, area.top);

  if (Browser* b = BrowserList::GetLastActive()) {
    RECT browser;
    HWND window = reinterpret_cast<HWND>(b->window()->GetNativeHandle());
    if (GetWindowRect(window, &browser)) {
      // Limit to not overflow the work area right and bottom edges.
      gfx::Point limit(
          std::min(browser.left + kWindowTilePixels, area.right-size.width()),
          std::min(browser.top + kWindowTilePixels, area.bottom-size.height())
      );
      // Adjust corner to now overflow the work area left and top edges, so
      // that if a popup does not fit the title-bar is remains visible.
      corner = gfx::Point(
          std::max(corner.x(), limit.x()),
          std::max(corner.y(), limit.y())
      );
    }
  }
  return corner;
}
