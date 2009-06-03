// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOCK_INFO_H_
#define CHROME_BROWSER_DOCK_INFO_H_

#include <set>

#include "base/gfx/native_widget_types.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"

// DockInfo is used to do determine possible dock locations for a dragged
// tab. To use DockInfo invoke GetDockInfoAtPoint. This returns a new
// DockInfo whose type indicates the type of dock that should occur based
// on the screen location. As the user drags the mouse around invoke
// IsValidForPoint, this returns true if the DockInfo is still valid for the
// new location. If the DockInfo is not valid, invoke GetDockInfoAtPoint to
// get the new DockInfo. Use GetNewWindowBounds to get the position to place
// the new window at.
//
// DockInfos are cheap and explicitly allow copy and assignment operators.
class DockInfo {
 public:
  class Factory {
   public:
    virtual DockInfo GetDockInfoAtPoint(
        const gfx::Point& screen_point,
        const std::set<gfx::NativeView>& ignore) = 0;

    virtual gfx::NativeWindow GetLocalProcessWindowAtPoint(
        const gfx::Point& screen_point,
        const std::set<gfx::NativeView>& ignore) = 0;
  };

  // Possible dock positions.
  enum Type {
    // Indicates there is no valid dock position for the current location.
    NONE,

    // Indicates the new window should be positioned relative to the window
    // identified by window().
    LEFT_OF_WINDOW,
    RIGHT_OF_WINDOW,
    BOTTOM_OF_WINDOW,
    TOP_OF_WINDOW,

    // Indicates the window should be maximized on the monitor at hot_spot.
    MAXIMIZE,

    // Indicates the window should be docked to a specific side of the monitor.
    LEFT_HALF,
    RIGHT_HALF,
    BOTTOM_HALF
  };

  DockInfo() : type_(NONE), window_(NULL), in_enable_area_(false) {}

  // Returns true if |screen_loc| is close to the hotspot at |x|, |y|. If the
  // point is close enough to the hotspot true is returned and |in_enable_area|
  // is set appropriately.
  static bool IsCloseToPoint(const gfx::Point& screen_loc,
                             int x,
                             int y,
                             bool* in_enable_area);

  // Variant of IsCloseToPoint used for monitor relative positions.
  static bool IsCloseToMonitorPoint(const gfx::Point& screen_loc,
                                    int x,
                                    int y,
                                    DockInfo::Type type,
                                    bool* in_enable_area);

  // Sets the factory.
  static void set_factory(Factory* factory) { factory_ = factory; }

  // Size of the popup window shown to indicate a valid dock location.
  static int popup_width();
  static int popup_height();

  // Returns the DockInfo for the specified point |screen_point|. |ignore|
  // contains the set of windows to ignore from consideration. This contains the
  // dragged window as well as any windows showing possible dock locations.
  //
  // If there is no docking position for the specified location the returned
  // DockInfo has a type of NONE.
  //
  // If a Factory has been set, the method of the same name is invoked on the
  // Factory to determine the DockInfo.
  static DockInfo GetDockInfoAtPoint(const gfx::Point& screen_point,
                                     const std::set<gfx::NativeView>& ignore);

  // Returns the top most window from the current process at |screen_point|.
  // See GetDockInfoAtPoint for a description of |ignore|. This returns NULL if
  // there is no window from the current process at |screen_point|, or another
  // window obscures the topmost window from our process at |screen_point|.
  //
  // If a Factory has been set, the method of the same name is invoked on the
  // Factory to determine the DockInfo.
  static gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeView>& ignore);

  // Returns true if this DockInfo is valid for the specified point. This
  // resets in_enable_area based on the new location.
  bool IsValidForPoint(const gfx::Point& screen_point);

  // Returns the bounds for the new window in |new_window_bounds|. If the new
  // window is to be maximized, |maximize_new_window| is set to true.
  // This returns true if type is other than NONE or the mouse isn't in the
  // enable area, false otherwise.
  bool GetNewWindowBounds(gfx::Rect* new_window_bounds,
                          bool* maximize_new_window) const;

  // Adjust the bounds of the other window during docking. Does nothing if type
  // is NONE, in_enable_are is false, or the type is not window relative.
  void AdjustOtherWindowBounds() const;

  // Type of docking to occur.
  void set_type(Type type) { type_ = type; }
  Type type() const { return type_; }

  // The window to dock too. Is null for dock types that are relative to the
  // monitor.
  void set_window(gfx::NativeWindow window) { window_ = window; }
  gfx::NativeWindow window() const { return window_; }

  // The location of the hotspot.
  void set_hot_spot(const gfx::Point& hot_spot) { hot_spot_ = hot_spot; }
  const gfx::Point& hot_spot() const { return hot_spot_; }

  // Bounds of the monitor.
  void set_monitor_bounds(const gfx::Rect& monitor_bounds) {
    monitor_bounds_ = monitor_bounds;
  }
  const gfx::Rect& monitor_bounds() const { return monitor_bounds_; }

  // Returns the bounds of the window to show the indicator for.
  gfx::Rect GetPopupRect() const;

  // Returns true if the drop should result in docking. DockInfo maintains two
  // states (as indicated by this boolean):
  // 1. The mouse is close enough to the hot spot such that a visual indicator
  //    should be shown, but if the user releases the mouse docking shouldn't
  //    result. This corresponds to a value of false for in_enable_area.
  // 2. The mouse is close enough to the hot spot such that releasing the mouse
  //    should result in docking. This corresponds to a value of true for
  //    in_enable_area.
  void set_in_enable_area(bool in_enable_area) {
    in_enable_area_ = in_enable_area;
  }
  bool in_enable_area() const { return in_enable_area_; }

  // Returns true if |other| is considered equal to this. Two DockInfos are
  // considered equal if they have the same type and same window.
  bool equals(const DockInfo& other) const {
    return type_ == other.type_ && window_ == other.window_ &&
           monitor_bounds_ == other.monitor_bounds_;
  }

  // If screen_loc is close enough to the hot spot given by |x| and |y|, the
  // type and hot_spot are set from the supplied parameters. This is used
  // internally, there is no need to invoke this otherwise.
  bool CheckMonitorPoint(const gfx::Point& screen_loc,
                         int x,
                         int y,
                         Type type);

 private:
  // Returns the bounds of the window.
  bool GetWindowBounds(gfx::Rect* bounds) const;
  void SizeOtherWindowTo(const gfx::Rect& bounds) const;

  Type type_;
  gfx::NativeWindow window_;
  gfx::Point hot_spot_;
  gfx::Rect monitor_bounds_;
  bool in_enable_area_;

  // Factory that creates DockInfos. By default this is NULL, which gives the
  // default behavior.
  static Factory* factory_;
};

#endif  // CHROME_BROWSER_DOCK_INFO_H_
