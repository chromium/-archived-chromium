// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dock_info.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab.h"

namespace {

// BaseWindowFinder -----------------------------------------------------------

// Base class used to locate a window. This is intended to be used with the
// various win32 functions that iterate over windows.
//
// A subclass need only override ShouldStopIterating to determine when
// iteration should stop.
class BaseWindowFinder {
 public:
  // Creates a BaseWindowFinder with the specified set of HWNDs to ignore.
  BaseWindowFinder(const std::set<HWND>& ignore) : ignore_(ignore) {}
  virtual ~BaseWindowFinder() {}

 protected:
  // Returns true if iteration should stop, false if iteration should continue.
  virtual bool ShouldStopIterating(HWND window) = 0;

  static BOOL CALLBACK WindowCallbackProc(HWND hwnd, LPARAM lParam) {
    BaseWindowFinder* finder = reinterpret_cast<BaseWindowFinder*>(lParam);
    if (finder->ignore_.find(hwnd) != finder->ignore_.end())
      return TRUE;

    return finder->ShouldStopIterating(hwnd) ? FALSE : TRUE;
  }

 private:
  const std::set<HWND>& ignore_;

  DISALLOW_COPY_AND_ASSIGN(BaseWindowFinder);
};

// TopMostFinder --------------------------------------------------------------

// Helper class to determine if a particular point of a window is not obscured
// by another window.
class TopMostFinder : public BaseWindowFinder {
 public:
  // Returns true if |window| is the topmost window at the location
  // |screen_loc|, not including the windows in |ignore|.
  static bool IsTopMostWindowAtPoint(HWND window,
                                     const gfx::Point& screen_loc,
                                     const std::set<HWND>& ignore) {
    TopMostFinder finder(window, screen_loc, ignore);
    return finder.is_top_most_;
  }

  virtual bool ShouldStopIterating(HWND hwnd) {
    if (hwnd == target_) {
      // Window is topmost, stop iterating.
      is_top_most_ = true;
      return true;
    }

    if (!::IsWindowVisible(hwnd)) {
      // The window isn't visible, keep iterating.
      return false;
    }

    CRect r;
    if (!::GetWindowRect(hwnd, &r) || !r.PtInRect(screen_loc_.ToPOINT())) {
      // The window doesn't contain the point, keep iterating.
      return false;
    }

    // hwnd is at the point. Make sure the point is within the windows region.
    if (GetWindowRgn(hwnd, tmp_region_.Get()) == ERROR) {
      // There's no region on the window and the window contains the point. Stop
      // iterating.
      return true;
    }

    // The region is relative to the window's rect.
    BOOL is_point_in_region = PtInRegion(tmp_region_.Get(),
        screen_loc_.x() - r.left, screen_loc_.y() - r.top);
    tmp_region_ = CreateRectRgn(0, 0, 0, 0);
    // Stop iterating if the region contains the point.
    return !!is_point_in_region;
  }

 private:
  TopMostFinder(HWND window,
                const gfx::Point& screen_loc,
                const std::set<HWND>& ignore)
      : BaseWindowFinder(ignore),
        target_(window),
        screen_loc_(screen_loc),
        is_top_most_(false),
        tmp_region_(CreateRectRgn(0, 0, 0, 0)) {
    EnumWindows(WindowCallbackProc, reinterpret_cast<LPARAM>(this));
  }

  // The window we're looking for.
  HWND target_;

  // Location of window to find.
  gfx::Point screen_loc_;

  // Is target_ the top most window? This is initially false but set to true
  // in ShouldStopIterating if target_ is passed in.
  bool is_top_most_;

  ScopedHRGN tmp_region_;

  DISALLOW_COPY_AND_ASSIGN(TopMostFinder);
};

// WindowFinder ---------------------------------------------------------------

// Helper class to determine if a particular point contains a window from our
// process.
class LocalProcessWindowFinder : public BaseWindowFinder {
 public:
  // Returns the hwnd from our process at screen_loc that is not obscured by
  // another window. Returns NULL otherwise.
  static HWND GetProcessWindowAtPoint(const gfx::Point& screen_loc,
                                      const std::set<HWND>& ignore) {
    LocalProcessWindowFinder finder(screen_loc, ignore);
    if (finder.result_ &&
        TopMostFinder::IsTopMostWindowAtPoint(finder.result_, screen_loc,
                                              ignore)) {
      return finder.result_;
    }
    return NULL;
  }

 protected:
  virtual bool ShouldStopIterating(HWND hwnd) {
    CRect r;
    if (::IsWindowVisible(hwnd) && ::GetWindowRect(hwnd, &r) &&
        r.PtInRect(screen_loc_.ToPOINT())) {
      result_ = hwnd;
      return true;
    }
    return false;
  }

 private:
  LocalProcessWindowFinder(const gfx::Point& screen_loc,
                           const std::set<HWND>& ignore)
      : BaseWindowFinder(ignore),
        screen_loc_(screen_loc),
        result_(NULL) {
    EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc,
                      reinterpret_cast<LPARAM>(this));
  }

  // Position of the mouse.
  gfx::Point screen_loc_;

  // The resulting window. This is initially null but set to true in
  // ShouldStopIterating if an appropriate window is found.
  HWND result_;

  DISALLOW_COPY_AND_ASSIGN(LocalProcessWindowFinder);
};

// DockToWindowFinder ---------------------------------------------------------

// Helper class for creating a DockInfo from a specified point.
class DockToWindowFinder : public BaseWindowFinder {
 public:
  // Returns the DockInfo for the specified point. If there is no docking
  // position for the specified point the returned DockInfo has a type of NONE.
  static DockInfo GetDockInfoAtPoint(const gfx::Point& screen_loc,
                                     const std::set<HWND>& ignore) {
    DockToWindowFinder finder(screen_loc, ignore);
    if (!finder.result_.window() ||
        !TopMostFinder::IsTopMostWindowAtPoint(finder.result_.window(),
                                               finder.result_.hot_spot(),
                                               ignore)) {
      finder.result_.set_type(DockInfo::NONE);
    }
    return finder.result_;
  }

 protected:
  virtual bool ShouldStopIterating(HWND hwnd) {
    BrowserView* window = BrowserView::GetBrowserViewForNativeWindow(hwnd);
    CRect bounds;
    if (!window || !::IsWindowVisible(hwnd) ||
        !::GetWindowRect(hwnd, &bounds)) {
      return false;
    }

    // Check the three corners we allow docking to. We don't currently allow
    // docking to top of window as it conflicts with docking to the tab strip.
    if (CheckPoint(hwnd, bounds.left, (bounds.top + bounds.bottom) / 2,
                   DockInfo::LEFT_OF_WINDOW) ||
        CheckPoint(hwnd, bounds.right - 1, (bounds.top + bounds.bottom) / 2,
                   DockInfo::RIGHT_OF_WINDOW) ||
        CheckPoint(hwnd, (bounds.left + bounds.right) / 2, bounds.bottom - 1,
                   DockInfo::BOTTOM_OF_WINDOW)) {
      return true;
    }
    return false;
  }

 private:
  DockToWindowFinder(const gfx::Point& screen_loc,
                     const std::set<HWND>& ignore)
      : BaseWindowFinder(ignore),
        screen_loc_(screen_loc) {
    HMONITOR monitor = MonitorFromPoint(screen_loc.ToPOINT(),
                                        MONITOR_DEFAULTTONULL);
    MONITORINFO monitor_info = {0};
    monitor_info.cbSize = sizeof(MONITORINFO);
    if (monitor && GetMonitorInfo(monitor, &monitor_info)) {
      result_.set_monitor_bounds(gfx::Rect(monitor_info.rcWork));
      EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc,
                        reinterpret_cast<LPARAM>(this));
    }
  }

  bool CheckPoint(HWND hwnd, int x, int y, DockInfo::Type type) {
    bool in_enable_area;
    if (DockInfo::IsCloseToPoint(screen_loc_, x, y, &in_enable_area)) {
      result_.set_in_enable_area(in_enable_area);
      result_.set_window(hwnd);
      result_.set_type(type);
      result_.set_hot_spot(gfx::Point(x, y));
      // Only show the hotspot if the monitor contains the bounds of the popup
      // window. Otherwise we end with a weird situation where the popup window
      // isn't completely visible.
      return result_.monitor_bounds().Contains(result_.GetPopupRect());
    }
    return false;
  }

  // The location to look for.
  gfx::Point screen_loc_;

  // The resulting DockInfo.
  DockInfo result_;

  DISALLOW_COPY_AND_ASSIGN(DockToWindowFinder);
};

}  // namespace

// DockInfo -------------------------------------------------------------------

// static
DockInfo DockInfo::GetDockInfoAtPoint(const gfx::Point& screen_point,
                                      const std::set<HWND>& ignore) {
  if (factory_)
    return factory_->GetDockInfoAtPoint(screen_point, ignore);

  // Try docking to a window first.
  DockInfo info = DockToWindowFinder::GetDockInfoAtPoint(screen_point, ignore);
  if (info.type() != DockInfo::NONE)
    return info;

  // No window relative positions. Try monitor relative positions.
  const gfx::Rect& m_bounds = info.monitor_bounds();
  int mid_x = m_bounds.x() + m_bounds.width() / 2;
  int mid_y = m_bounds.y() + m_bounds.height() / 2;

  bool result =
      info.CheckMonitorPoint(screen_point, mid_x, m_bounds.y(),
                             DockInfo::MAXIMIZE) ||
      info.CheckMonitorPoint(screen_point, mid_x, m_bounds.bottom(),
                             DockInfo::BOTTOM_HALF) ||
      info.CheckMonitorPoint(screen_point, m_bounds.x(), mid_y,
                             DockInfo::LEFT_HALF) ||
      info.CheckMonitorPoint(screen_point, m_bounds.right(), mid_y,
                             DockInfo::RIGHT_HALF);

  return info;
}

HWND DockInfo::GetLocalProcessWindowAtPoint(const gfx::Point& screen_point,
                                            const std::set<HWND>& ignore) {
  if (factory_)
    return factory_->GetLocalProcessWindowAtPoint(screen_point, ignore);
  return
      LocalProcessWindowFinder::GetProcessWindowAtPoint(screen_point, ignore);
}

bool DockInfo::GetWindowBounds(gfx::Rect* bounds) const {
  RECT window_rect;
  if (!window() || !GetWindowRect(window(), &window_rect))
    return false;
  *bounds = gfx::Rect(window_rect);
  return true;
}

void DockInfo::SizeOtherWindowTo(const gfx::Rect& bounds) const {
  if (IsZoomed(window())) {
    // We're docking relative to another window, we need to make sure the
    // window we're docking to isn't maximized.
    ShowWindow(window(), SW_RESTORE | SW_SHOWNA);
  }
  ::SetWindowPos(window(), HWND_TOP, bounds.x(), bounds.y(), bounds.width(),
                 bounds.height(), SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}
