// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/window_sizer.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

// How much horizontal and vertical offset there is between newly opened
// windows.
static const int kWindowTilePixels = 10;

///////////////////////////////////////////////////////////////////////////////
// An implementation of WindowSizer::MonitorInfoProvider that gets the actual
// monitor information from Windows.
class DefaultMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  DefaultMonitorInfoProvider() {
    EnumDisplayMonitors(NULL, NULL,
                        &DefaultMonitorInfoProvider::MonitorEnumProc,
                        reinterpret_cast<LPARAM>(&working_rects_));
  }

  // Overridden from WindowSizer::MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryMonitorWorkingRect() const {
    return gfx::Rect(GetMonitorInfoForMonitor(MonitorFromWindow(NULL,
        MONITOR_DEFAULTTOPRIMARY)).rcWork);
  }

  virtual gfx::Rect GetPrimaryMonitorBounds() const {
    return gfx::Rect(GetMonitorInfoForMonitor(MonitorFromWindow(NULL,
        MONITOR_DEFAULTTOPRIMARY)).rcMonitor);
  }

  virtual gfx::Rect GetMonitorWorkingRectMatching(
      const gfx::Rect& match_rect) const {
    CRect other_bounds_crect = match_rect.ToRECT();
    MONITORINFO monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
        &other_bounds_crect, MONITOR_DEFAULTTOPRIMARY));
    return gfx::Rect(monitor_info.rcWork);
  }

  virtual gfx::Point GetBoundsOffsetMatching(
      const gfx::Rect& match_rect) const {
    CRect other_bounds_crect = match_rect.ToRECT();
    MONITORINFO monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
        &other_bounds_crect, MONITOR_DEFAULTTOPRIMARY));
    return gfx::Point(monitor_info.rcWork.left - monitor_info.rcMonitor.left,
                      monitor_info.rcWork.top - monitor_info.rcMonitor.top);
  }

  virtual int GetMonitorCount() const {
    return static_cast<int>(working_rects_.size());
  }

  virtual gfx::Rect GetWorkingRectAt(int index) const {
    DCHECK(index >= 0 && index < GetMonitorCount());
    return working_rects_.at(index);
  }

 private:
  // A callback for EnumDisplayMonitors that records the work area of the
  // current monitor in the enumeration.
  static BOOL CALLBACK MonitorEnumProc(HMONITOR monitor,
                                       HDC monitor_dc,
                                       LPRECT monitor_rect,
                                       LPARAM data) {
    std::vector<gfx::Rect>* working_rects =
        reinterpret_cast<std::vector<gfx::Rect>*>(data);
    MONITORINFO info;
    info.cbSize = sizeof(info);
    GetMonitorInfo(monitor, &info);
    working_rects->push_back(gfx::Rect(info.rcWork));
    return TRUE;
  }

  static MONITORINFO GetMonitorInfoForMonitor(HMONITOR monitor) {
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(monitor, &monitor_info);
    return monitor_info;
  }

  std::vector<gfx::Rect> working_rects_;

  DISALLOW_EVIL_CONSTRUCTORS(DefaultMonitorInfoProvider);
};

///////////////////////////////////////////////////////////////////////////////
// An implementation of WindowSizer::StateProvider that gets the last active
// and persistent state from the browser window and the user's profile.
class DefaultStateProvider : public WindowSizer::StateProvider {
 public:
  explicit DefaultStateProvider(const std::wstring& app_name)
      : app_name_(app_name) {
  }

  // Overridden from WindowSizer::StateProvider:
  virtual bool GetPersistentState(gfx::Rect* bounds, bool* maximized) const {
    DCHECK(bounds && maximized);

    std::wstring key(prefs::kBrowserWindowPlacement);
    if (!app_name_.empty()) {
      key.append(L"_");
      key.append(app_name_);
    }

    if (!g_browser_process->local_state())
      return false;

    const DictionaryValue* wp_pref =
        g_browser_process->local_state()->GetDictionary(key.c_str());
    int top = 0, left = 0, bottom = 0, right = 0;
    bool has_prefs =
        wp_pref &&
        wp_pref->GetInteger(L"top", &top) &&
        wp_pref->GetInteger(L"left", &left) &&
        wp_pref->GetInteger(L"bottom", &bottom) &&
        wp_pref->GetInteger(L"right", &right) &&
        wp_pref->GetBoolean(L"maximized", maximized);
    bounds->SetRect(left, top, std::max(0, right - left),
                    std::max(0, bottom - top));
    return has_prefs;
  }

  virtual bool GetLastActiveWindowState(gfx::Rect* bounds) const {
    // Applications are always restored with the same position.
    if (!app_name_.empty())
      return false;

    BrowserList::const_reverse_iterator it = BrowserList::begin_last_active();
    BrowserList::const_reverse_iterator end = BrowserList::end_last_active();
    for (; it != end; ++it) {
      Browser* last_active = *it;
      if (last_active && last_active->type() == Browser::TYPE_NORMAL) {
        BrowserWindow* frame = last_active->window();
        DCHECK(frame);
        *bounds = frame->GetNormalBounds();
        return true;
      }
    }

    return false;
  }

 private:
  std::wstring app_name_;

  DISALLOW_EVIL_CONSTRUCTORS(DefaultStateProvider);
};

///////////////////////////////////////////////////////////////////////////////
// WindowSizer, public:

WindowSizer::WindowSizer(
    StateProvider* state_provider,
    MonitorInfoProvider* monitor_info_provider) {
  Init(state_provider, monitor_info_provider);
}

WindowSizer::~WindowSizer() {
  if (state_provider_)
    delete state_provider_;
  if (monitor_info_provider_)
    delete monitor_info_provider_;
}

// static
void WindowSizer::GetBrowserWindowBounds(const std::wstring& app_name,
                                         const gfx::Rect& specified_bounds,
                                         gfx::Rect* window_bounds,
                                         bool* maximized) {
  const WindowSizer sizer(new DefaultStateProvider(app_name),
                          new DefaultMonitorInfoProvider);
  sizer.DetermineWindowBounds(specified_bounds, window_bounds, maximized);
}


///////////////////////////////////////////////////////////////////////////////
// WindowSizer, private:

WindowSizer::WindowSizer(const std::wstring& app_name) {
  Init(new DefaultStateProvider(app_name),
       new DefaultMonitorInfoProvider);
}

void WindowSizer::Init(StateProvider* state_provider,
                       MonitorInfoProvider* monitor_info_provider) {
  state_provider_ = state_provider;
  monitor_info_provider_ = monitor_info_provider;
}

void WindowSizer::DetermineWindowBounds(const gfx::Rect& specified_bounds,
                                        gfx::Rect* bounds,
                                        bool* maximized) const {
  *bounds = specified_bounds;
  if (bounds->IsEmpty()) {
    // See if there's saved placement information.
    if (!GetLastWindowBounds(bounds)) {
      if (!GetSavedWindowBounds(bounds, maximized)) {
        // No saved placement, figure out some sensible default size based on
        // the user's screen size.
        GetDefaultWindowBounds(bounds);
      }
    }
  }
}

bool WindowSizer::GetLastWindowBounds(gfx::Rect* bounds) const {
  DCHECK(bounds);
  if (!state_provider_ || !state_provider_->GetLastActiveWindowState(bounds))
    return false;
  gfx::Rect last_window_bounds = *bounds;
  bounds->Offset(kWindowTilePixels, kWindowTilePixels);
  AdjustBoundsToBeVisibleOnMonitorContaining(last_window_bounds, bounds);
  return true;
}

bool WindowSizer::GetSavedWindowBounds(gfx::Rect* bounds,
                                       bool* maximized) const {
  DCHECK(bounds && maximized);
  if (!state_provider_ ||
      !state_provider_->GetPersistentState(bounds, maximized))
    return false;
  const gfx::Point& taskbar_offset =
      monitor_info_provider_->GetBoundsOffsetMatching(*bounds);
  bounds->Offset(taskbar_offset.x(), taskbar_offset.y());
  AdjustBoundsToBeVisibleOnMonitorContaining(*bounds, bounds);
  return true;
}

void WindowSizer::GetDefaultWindowBounds(gfx::Rect* default_bounds) const {
  DCHECK(default_bounds);
  DCHECK(monitor_info_provider_);

  gfx::Rect work_rect = monitor_info_provider_->GetPrimaryMonitorWorkingRect();

  // The default size is either some reasonably wide width, or if the work
  // area is narrower, then the work area width less some aesthetic padding.
  int default_width = std::min(work_rect.width() - 2 * kWindowTilePixels, 1050);
  int default_height = work_rect.height() - 2 * kWindowTilePixels;

  // For wider aspect ratio displays at higher resolutions, we might size the
  // window narrower to allow two windows to easily be placed side-by-side.
  gfx::Rect screen_size = monitor_info_provider_->GetPrimaryMonitorBounds();
  double width_to_height =
    static_cast<double>(screen_size.width()) / screen_size.height();

  // The least wide a screen can be to qualify for the halving described above.
  static const int kMinScreenWidthForWindowHalving = 1600;
  // We assume 16:9/10 is a fairly standard indicator of a wide aspect ratio
  // computer display.
  if (((width_to_height * 10) >= 16) &&
      work_rect.width() > kMinScreenWidthForWindowHalving) {
    // Halve the work area, subtracting aesthetic padding on either side, plus
    // some more aesthetic padding for spacing between windows.
    default_width = (work_rect.width() / 2) - 3 * kWindowTilePixels;
  }
  default_bounds->SetRect(kWindowTilePixels + work_rect.x(),
                          kWindowTilePixels + work_rect.y(),
                          default_width, default_height);
}

bool WindowSizer::PositionIsOffscreen(int position, Edge edge) const {
  DCHECK(monitor_info_provider_);

  int monitor_count = monitor_info_provider_->GetMonitorCount();
  for (int i = 0; i < monitor_count; ++i) {
    gfx::Rect working_rect = monitor_info_provider_->GetWorkingRectAt(i);
    switch (edge) {
      case TOP:
        if (position >= working_rect.y())
          return true;
        break;
      case LEFT:
        if (position >= working_rect.x())
          return true;
        break;
      case BOTTOM:
        if (position <= working_rect.height())
          return true;
        break;
      case RIGHT:
        if (position <= working_rect.width())
          return true;
        break;
    }
  }
  return false;
}

void WindowSizer::AdjustBoundsToBeVisibleOnMonitorContaining(
    const gfx::Rect& other_bounds, gfx::Rect* bounds) const {
  DCHECK(bounds);
  DCHECK(monitor_info_provider_);

  // Find the size of the work area of the monitor that intersects the bounds
  // of the anchor window.
  gfx::Rect work_area =
      monitor_info_provider_->GetMonitorWorkingRectMatching(other_bounds);

  // If height or width are 0, reset to the default size.
  gfx::Rect default_bounds;
  GetDefaultWindowBounds(&default_bounds);
  if (bounds->height() <= 0)
    bounds->set_height(default_bounds.height());
  if (bounds->width() <= 0)
    bounds->set_width(default_bounds.width());

  // First determine which screen edge(s) the window is offscreen on.
  bool top_offscreen = !PositionIsOffscreen(bounds->y(), TOP);
  bool left_offscreen = !PositionIsOffscreen(bounds->x(), LEFT);
  bool bottom_offscreen =  !PositionIsOffscreen(bounds->bottom(), BOTTOM);
  bool right_offscreen = !PositionIsOffscreen(bounds->right(), RIGHT);

  // Bump the window back onto the screen in the direction that it's offscreen.
  if (bottom_offscreen) {
    int y = work_area.bottom() - kWindowTilePixels - bounds->height();
    bounds->set_y(std::max(kWindowTilePixels, y));
  }
  if (right_offscreen) {
    int x = work_area.right() - kWindowTilePixels - bounds->width();
    bounds->set_x(std::max(kWindowTilePixels, x));
  }
  if (top_offscreen)
    bounds->set_y(kWindowTilePixels + work_area.y());
  if (left_offscreen)
    bounds->set_x(kWindowTilePixels + work_area.x());

  // Now that we've tried to correct the x/y position to something reasonable,
  // see if the window is still too tall or wide to fit, and resize it if need
  // be.
  if ((bottom_offscreen || top_offscreen) &&
      bounds->bottom() > work_area.bottom()) {
    bounds->set_height(work_area.height() - 2 * kWindowTilePixels);
  }
  if ((left_offscreen || right_offscreen) &&
      bounds->right() > work_area.right()) {
    bounds->set_width(work_area.width() - 2 * kWindowTilePixels);
  }
}

