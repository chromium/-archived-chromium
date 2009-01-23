// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dock_info.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/frame/browser_view.h"

namespace {

// Distance in pixels between the hotspot and when the hint should be shown.
const int kHotSpotDeltaX = 100;
const int kHotSpotDeltaY = 80;

// Distance in pixels between the hotspot and when the hint should be shown
// and enabled.
const int kEnableDeltaX = 50;
const int kEnableDeltaY = 37;

// Distance used when maximizing. The maximize area is the whole top of the
// monitor.
const int kMaximizeHotSpotDeltaY = 100;
const int kMaximizeEnableDeltaY = 50;

// Returns true if |screen_loc| is close to the hotspot at |x|, |y|. If the
// point is close enough to the hotspot true is returned and |in_enable_area|
// is set appropriately.
bool IsCloseToPoint(const gfx::Point& screen_loc,
                    int x,
                    int y,
                    bool* in_enable_area) {
  int delta_x = abs(x - screen_loc.x());
  int delta_y = abs(y - screen_loc.y());
  *in_enable_area = (delta_x < kEnableDeltaX && delta_y < kEnableDeltaY);
  return *in_enable_area || (delta_x < kHotSpotDeltaX &&
                             delta_y < kHotSpotDeltaY);
}

// Variant of IsCloseToPoint used for monitor relative positions.
bool IsCloseToMonitorPoint(const gfx::Point& screen_loc,
                           int x,
                           int y,
                           DockInfo::Type type,
                           bool* in_enable_area) {
  // Because the monitor relative positions are aligned with the edge of the
  // monitor these need to be handled differently.
  int delta_x = abs(x - screen_loc.x());
  int delta_y = abs(y - screen_loc.y());

  int enable_delta_x = kEnableDeltaX;
  int enable_delta_y = kEnableDeltaY;
  int hot_spot_delta_x = kHotSpotDeltaX;
  int hot_spot_delta_y = kHotSpotDeltaY;

  switch (type) {
    case DockInfo::LEFT_HALF:
    case DockInfo::RIGHT_HALF:
      enable_delta_x += enable_delta_x;
      hot_spot_delta_x += hot_spot_delta_x;
      break;


    case DockInfo::MAXIMIZE:
    case DockInfo::BOTTOM_HALF:
      enable_delta_y += enable_delta_y;
      hot_spot_delta_y += hot_spot_delta_y;
      break;

    default:
      NOTREACHED();
      return false;
  }
  *in_enable_area = (delta_x < enable_delta_x && delta_y < enable_delta_y);
  bool result = (*in_enable_area || (delta_x < hot_spot_delta_x &&
                                     delta_y < hot_spot_delta_y));
  if (type != DockInfo::MAXIMIZE)
    return result;

  // Make the maximize area the whole top of the monitor.
  int max_delta_y = abs(screen_loc.y() - y);
  *in_enable_area = (*in_enable_area || (max_delta_y < kMaximizeEnableDeltaY));
  return *in_enable_area || (max_delta_y < kMaximizeHotSpotDeltaY);
}

// Returns true if there is a maximized tabbed browser on the monitor
// |monitor|.
bool IsMaximizedTabbedBrowserOnMonitor(HMONITOR monitor) {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    Browser* browser = *i;
    if (browser->type() == Browser::TYPE_NORMAL) {
      HWND browser_hwnd =
          reinterpret_cast<HWND>(browser->window()->GetNativeHandle());
      if (IsZoomed(browser_hwnd) &&
          MonitorFromWindow(browser_hwnd, MONITOR_DEFAULTTONEAREST) ==
          monitor) {
        return true;
      }
    }
  }
  return false;
}

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

    if (::IsWindowVisible(hwnd)) {
      CRect r;
      if (::GetWindowRect(hwnd, &r) && r.PtInRect(screen_loc_.ToPOINT())) {
        // Not the topmost, stop iterating.
        return true;
      }
    }
    return false;
  }

 private:
  TopMostFinder(HWND window,
                const gfx::Point& screen_loc,
                const std::set<HWND>& ignore)
      : BaseWindowFinder(ignore),
        target_(window),
        screen_loc_(screen_loc),
        is_top_most_(false) {
    EnumWindows(WindowCallbackProc, reinterpret_cast<LPARAM>(this));
  }

  // The window we're looking for.
  HWND target_;

  // Location of window to find.
  gfx::Point screen_loc_;

  // Is target_ the top most window? This is initially false but set to true
  // in ShouldStopIterating if target_ is passed in.
  bool is_top_most_;

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
    if (!finder.result_.hwnd() ||
        !TopMostFinder::IsTopMostWindowAtPoint(finder.result_.hwnd(),
                                               finder.result_.hot_spot(),
                                               ignore)) {
      return DockInfo();
    }
    return finder.result_;
  }

 protected:
  virtual bool ShouldStopIterating(HWND hwnd) {
    BrowserView* window = BrowserView::GetBrowserViewForHWND(hwnd);
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
    EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc,
                      reinterpret_cast<LPARAM>(this));
  }

  bool CheckPoint(HWND hwnd, int x, int y, DockInfo::Type type) {
    bool in_enable_area;
    if (IsCloseToPoint(screen_loc_, x, y, &in_enable_area)) {
      result_.set_in_enable_area(in_enable_area);
      result_.set_hwnd(hwnd);
      result_.set_type(type);
      result_.set_hot_spot(gfx::Point(x, y));
      return true;
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

DockInfo DockInfo::GetDockInfoAtPoint(const gfx::Point& screen_point,
                                      const std::set<HWND>& ignore) {
  // Try docking to a window first.
  DockInfo info = DockToWindowFinder::GetDockInfoAtPoint(screen_point, ignore);

  HMONITOR monitor = MonitorFromPoint(screen_point.ToPOINT(),
                                      MONITOR_DEFAULTTONULL);
  MONITORINFO monitor_info = {0};
  monitor_info.cbSize = sizeof(MONITORINFO);
  if (!monitor || !GetMonitorInfo(monitor, &monitor_info)) {
    info.type_ = NONE;
    return info;
  }
  info.monitor_bounds_ = gfx::Rect(monitor_info.rcWork);

  if (info.type() != DockInfo::NONE)
    return info;

  // No window relative positions. Try monitor relative positions.
  RECT& m_bounds = monitor_info.rcWork;
  int mid_x = (m_bounds.left + m_bounds.right) / 2;
  int mid_y = (m_bounds.top + m_bounds.bottom) / 2;

  bool result = 
      info.CheckMonitorPoint(monitor, screen_point, mid_x, m_bounds.top,
                             DockInfo::MAXIMIZE) ||
      info.CheckMonitorPoint(monitor, screen_point, mid_x, m_bounds.bottom,
                             DockInfo::BOTTOM_HALF) ||
      info.CheckMonitorPoint(monitor, screen_point, m_bounds.left, mid_y,
                             DockInfo::LEFT_HALF) ||
      info.CheckMonitorPoint(monitor, screen_point, m_bounds.right, mid_y,
                             DockInfo::RIGHT_HALF);

  return info;
}

HWND DockInfo::GetLocalProcessWindowAtPoint(const gfx::Point& screen_point,
                                            const std::set<HWND>& ignore) {
  return
      LocalProcessWindowFinder::GetProcessWindowAtPoint(screen_point, ignore);
}

bool DockInfo::IsValidForPoint(const gfx::Point& screen_point) {
  if (type_ == NONE)
    return false;

  if (hwnd_) {
    return IsCloseToPoint(screen_point, hot_spot_.x(), hot_spot_.y(),
                          &in_enable_area_);
  }

  return monitor_bounds_.Contains(screen_point) &&
          IsCloseToMonitorPoint(screen_point, hot_spot_.x(),
                                hot_spot_.y(), type_, &in_enable_area_);
}

bool DockInfo::GetNewWindowBounds(gfx::Rect* new_window_bounds,
                                  bool* maximize_new_window) const {
  if (type_ == NONE || !in_enable_area_)
    return false;

  RECT window_rect;
  if (hwnd_ && !GetWindowRect(hwnd_, &window_rect))
    return false;

  int half_m_width = (monitor_bounds_.right() - monitor_bounds_.x()) / 2;
  int half_m_height = (monitor_bounds_.bottom() - monitor_bounds_.y()) / 2;
  bool unmaximize_other_window = false;

  *maximize_new_window = false;

  switch (type_) {
    case LEFT_OF_WINDOW:
      new_window_bounds->SetRect(monitor_bounds_.x(), window_rect.top,
          half_m_width, window_rect.bottom - window_rect.top);
      break;

    case RIGHT_OF_WINDOW:
      new_window_bounds->SetRect(monitor_bounds_.x() + half_m_width,
          window_rect.top, half_m_width, window_rect.bottom - window_rect.top);
      break;

    case TOP_OF_WINDOW:
      new_window_bounds->SetRect(window_rect.left, monitor_bounds_.y(),
                                 window_rect.right - window_rect.left,
                                 half_m_height);
      break;

    case BOTTOM_OF_WINDOW:
      new_window_bounds->SetRect(window_rect.left,
                                 monitor_bounds_.y() + half_m_height,
                                 window_rect.right - window_rect.left,
                                 half_m_height);
      break;

    case LEFT_HALF:
      new_window_bounds->SetRect(monitor_bounds_.x(), monitor_bounds_.y(),
                                 half_m_width, monitor_bounds_.height());
      break;

    case RIGHT_HALF:
      new_window_bounds->SetRect(monitor_bounds_.right() - half_m_width,
          monitor_bounds_.y(), half_m_width, monitor_bounds_.height());
      break;

    case BOTTOM_HALF:
      new_window_bounds->SetRect(monitor_bounds_.x(),
                                 monitor_bounds_.y() + half_m_height,
                                 monitor_bounds_.width(), half_m_height);
      break;

    case MAXIMIZE:
      *maximize_new_window = true;
      break;

    default:
      NOTREACHED();
  }
  return true;
}

void DockInfo::AdjustOtherWindowBounds() const {
  if (!in_enable_area_)
    return;

  RECT window_rect;
  if (!hwnd_ || !GetWindowRect(hwnd_, &window_rect))
    return;

  gfx::Rect other_window_bounds;
  int half_m_width = (monitor_bounds_.right() - monitor_bounds_.x()) / 2;
  int half_m_height = (monitor_bounds_.bottom() - monitor_bounds_.y()) / 2;

  switch (type_) {
    case LEFT_OF_WINDOW:
      other_window_bounds.SetRect(monitor_bounds_.x() + half_m_width,
          window_rect.top, half_m_width, window_rect.bottom - window_rect.top);
      break;

    case RIGHT_OF_WINDOW:
      other_window_bounds.SetRect(monitor_bounds_.x(), window_rect.top,
          half_m_width, window_rect.bottom - window_rect.top);
      break;

    case TOP_OF_WINDOW:
      other_window_bounds.SetRect(window_rect.left,
                                  monitor_bounds_.y() + half_m_height,
                                  window_rect.right - window_rect.left,
                                  half_m_height);
      break;

    case BOTTOM_OF_WINDOW:
      other_window_bounds.SetRect(window_rect.left, monitor_bounds_.y(),
                                  window_rect.right - window_rect.left,
                                  half_m_height);
      break;

    default:
      return;
  }

  if (IsZoomed(hwnd_)) {
    // We're docking relative to another window, we need to make sure the
    // window we're docking to isn't maximized.
    ShowWindow(hwnd_, SW_RESTORE | SW_SHOWNA);
  }
  ::SetWindowPos(hwnd_, HWND_TOP, other_window_bounds.x(),
                 other_window_bounds.y(), other_window_bounds.width(),
                 other_window_bounds.height(),
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

bool DockInfo::CheckMonitorPoint(HMONITOR monitor,
                                 const gfx::Point& screen_loc,
                                 int x,
                                 int y,
                                 Type type) {
  if (IsCloseToMonitorPoint(screen_loc, x, y, type, &in_enable_area_) &&
      (type != MAXIMIZE ||
       !IsMaximizedTabbedBrowserOnMonitor(monitor))) {
    hot_spot_.SetPoint(x, y);
    type_ = type;
    return true;
  }
  return false;
}
