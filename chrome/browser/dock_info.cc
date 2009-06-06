// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dock_info.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/tabs/tab.h"
#else
#include "chrome/browser/gtk/tabs/tab_gtk.h"
#endif

namespace {

// Distance in pixels between the hotspot and when the hint should be shown.
const int kHotSpotDeltaX = 120;
const int kHotSpotDeltaY = 120;

// Size of the popup window.
const int kPopupWidth = 70;
const int kPopupHeight = 70;

}  // namespace

// static
DockInfo::Factory* DockInfo::factory_ = NULL;

// static
bool DockInfo::IsCloseToPoint(const gfx::Point& screen_loc,
                              int x,
                              int y,
                              bool* in_enable_area) {
  int delta_x = abs(x - screen_loc.x());
  int delta_y = abs(y - screen_loc.y());
  *in_enable_area = (delta_x < kPopupWidth / 2 && delta_y < kPopupHeight / 2);
  return *in_enable_area || (delta_x < kHotSpotDeltaX &&
                             delta_y < kHotSpotDeltaY);
}

// static
bool DockInfo::IsCloseToMonitorPoint(const gfx::Point& screen_loc,
                                     int x,
                                     int y,
                                     DockInfo::Type type,
                                     bool* in_enable_area) {
  // Because the monitor relative positions are aligned with the edge of the
  // monitor these need to be handled differently.
  int delta_x = abs(x - screen_loc.x());
  int delta_y = abs(y - screen_loc.y());

  int enable_delta_x = kPopupWidth / 2;
  int enable_delta_y = kPopupHeight / 2;
  int hot_spot_delta_x = kHotSpotDeltaX;
  int hot_spot_delta_y = kHotSpotDeltaY;

  switch (type) {
    case DockInfo::LEFT_HALF:
    case DockInfo::RIGHT_HALF:
      enable_delta_x += enable_delta_x;
      hot_spot_delta_x += hot_spot_delta_x;
      break;


    case DockInfo::MAXIMIZE: {
      // Make the maximize height smaller than the tab height to avoid showing
      // the dock indicator when close to maximized browser.
#if defined(TOOLKIT_VIEWS)
      hot_spot_delta_y = Tab::GetMinimumUnselectedSize().height() - 1;
#else
      hot_spot_delta_y = TabGtk::GetMinimumUnselectedSize().height() - 1;
#endif
      enable_delta_y = hot_spot_delta_y / 2;
      break;
    }
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

  // Make the hot spot/enable spot for maximized windows the whole top of the
  // monitor.
  int max_delta_y = abs(screen_loc.y() - y);
  *in_enable_area = (*in_enable_area || (max_delta_y < enable_delta_y));
  return *in_enable_area || (max_delta_y < hot_spot_delta_y);
}

// static
int DockInfo::popup_width() {
  return kPopupWidth;
}

// static
int DockInfo::popup_height() {
  return kPopupHeight;
}

bool DockInfo::IsValidForPoint(const gfx::Point& screen_point) {
  if (type_ == NONE)
    return false;

  if (window_) {
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

  gfx::Rect window_bounds;
  if (window_ && !GetWindowBounds(&window_bounds))
    return false;

  int half_m_width = (monitor_bounds_.right() - monitor_bounds_.x()) / 2;
  int half_m_height = (monitor_bounds_.bottom() - monitor_bounds_.y()) / 2;

  *maximize_new_window = false;

  switch (type_) {
    case LEFT_OF_WINDOW:
      new_window_bounds->SetRect(monitor_bounds_.x(), window_bounds.y(),
                                 half_m_width, window_bounds.height());
      break;

    case RIGHT_OF_WINDOW:
      new_window_bounds->SetRect(monitor_bounds_.x() + half_m_width,
                                 window_bounds.y(), half_m_width,
                                 window_bounds.height());
      break;

    case TOP_OF_WINDOW:
      new_window_bounds->SetRect(window_bounds.x(), monitor_bounds_.y(),
                                 window_bounds.width(), half_m_height);
      break;

    case BOTTOM_OF_WINDOW:
      new_window_bounds->SetRect(window_bounds.x(),
                                 monitor_bounds_.y() + half_m_height,
                                 window_bounds.width(), half_m_height);
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

  gfx::Rect window_bounds;
  if (!window_ || !GetWindowBounds(&window_bounds))
    return;

  gfx::Rect other_window_bounds;
  int half_m_width = (monitor_bounds_.right() - monitor_bounds_.x()) / 2;
  int half_m_height = (monitor_bounds_.bottom() - monitor_bounds_.y()) / 2;

  switch (type_) {
    case LEFT_OF_WINDOW:
      other_window_bounds.SetRect(monitor_bounds_.x() + half_m_width,
                                  window_bounds.y(), half_m_width,
                                  window_bounds.height());
      break;

    case RIGHT_OF_WINDOW:
      other_window_bounds.SetRect(monitor_bounds_.x(), window_bounds.y(),
                                  half_m_width, window_bounds.height());
      break;

    case TOP_OF_WINDOW:
      other_window_bounds.SetRect(window_bounds.x(),
                                  monitor_bounds_.y() + half_m_height,
                                  window_bounds.width(), half_m_height);
      break;

    case BOTTOM_OF_WINDOW:
      other_window_bounds.SetRect(window_bounds.x(), monitor_bounds_.y(),
                                  window_bounds.width(), half_m_height);
      break;

    default:
      return;
  }

  SizeOtherWindowTo(other_window_bounds);
}

gfx::Rect DockInfo::GetPopupRect() const {
  int x = hot_spot_.x() - popup_width() / 2;
  int y = hot_spot_.y() - popup_height() / 2;
  switch (type_) {
    case LEFT_OF_WINDOW:
    case RIGHT_OF_WINDOW:
    case TOP_OF_WINDOW:
    case BOTTOM_OF_WINDOW: {
      // Constrain the popup to the monitor's bounds.
      gfx::Rect ideal_bounds(x, y, popup_width(), popup_height());
      ideal_bounds = ideal_bounds.AdjustToFit(monitor_bounds_);
      return ideal_bounds;
    }
    case DockInfo::MAXIMIZE:
      y += popup_height() / 2;
      break;
    case DockInfo::LEFT_HALF:
      x += popup_width() / 2;
      break;
    case DockInfo::RIGHT_HALF:
      x -= popup_width() / 2;
      break;
    case DockInfo::BOTTOM_HALF:
      y -= popup_height() / 2;
      break;

    default:
      NOTREACHED();
  }
  return gfx::Rect(x, y, popup_width(), popup_height());
}

bool DockInfo::CheckMonitorPoint(const gfx::Point& screen_loc,
                                 int x,
                                 int y,
                                 Type type) {
  if (IsCloseToMonitorPoint(screen_loc, x, y, type, &in_enable_area_)) {
    hot_spot_.SetPoint(x, y);
    type_ = type;
    return true;
  }
  return false;
}
