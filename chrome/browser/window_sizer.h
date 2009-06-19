// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WINDOW_SIZER_H_
#define CHROME_BROWSER_WINDOW_SIZER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class Browser;

///////////////////////////////////////////////////////////////////////////////
// WindowSizer
//
//  A class that determines the best new size and position for a window to be
//  shown at based several factors, including the position and size of the last
//  window of the same type, the last saved bounds of the window from the
//  previous session, and default system metrics if neither of the above two
//  conditions exist. The system has built-in providers for monitor metrics
//  and persistent storage (using preferences) but can be overrided with mocks
//  for testing.
//
class WindowSizer {
 public:
  class MonitorInfoProvider;
  class StateProvider;

  // The WindowSizer assumes ownership of these objects.
  WindowSizer(StateProvider* state_provider,
              MonitorInfoProvider* monitor_info_provider);
  virtual ~WindowSizer();

  // Static factory methods to create default MonitorInfoProvider
  // instances.  The returned object is owned by the caller.
  static MonitorInfoProvider* CreateDefaultMonitorInfoProvider();

  // An interface implemented by an object that can retrieve information about
  // the monitors on the system.
  class MonitorInfoProvider {
   public:
    virtual ~MonitorInfoProvider() { }

    // Returns the bounds of the work area of the primary monitor.
    virtual gfx::Rect GetPrimaryMonitorWorkArea() const = 0;

    // Returns the bounds of the primary monitor.
    virtual gfx::Rect GetPrimaryMonitorBounds() const = 0;

    // Returns the bounds of the work area of the monitor that most closely
    // intersects the provided bounds.
    virtual gfx::Rect GetMonitorWorkAreaMatching(
        const gfx::Rect& match_rect) const = 0;

    // Returns the delta between the work area and the monitor bounds for the
    // monitor that most closely intersects the provided bounds.
    virtual gfx::Point GetBoundsOffsetMatching(
        const gfx::Rect& match_rect) const = 0;

    // Ensures number and coordinates of work areas are up-to-date.  You must
    // call this before calling either of the below functions, as work areas can
    // change while the program is running.
    virtual void UpdateWorkAreas() = 0;

    // Returns the number of monitors on the system.
    size_t GetMonitorCount() const {
      return work_areas_.size();
    }

    // Returns the bounds of the work area of the monitor at the specified
    // index.
    gfx::Rect GetWorkAreaAt(size_t index) const {
      return work_areas_[index];
    }

   protected:
    std::vector<gfx::Rect> work_areas_;
  };

  // An interface implemented by an object that can retrieve state from either a
  // persistent store or an existing window.
  class StateProvider {
   public:
    virtual ~StateProvider() { }

    // Retrieve the persisted bounds of the window. Returns true if there was
    // persisted data to retrieve state information, false otherwise.
    virtual bool GetPersistentState(gfx::Rect* bounds,
                                    bool* maximized) const = 0;

    // Retrieve the bounds of the most recent window of the matching type.
    // Returns true if there was a last active window to retrieve state
    // information from, false otherwise.
    virtual bool GetLastActiveWindowState(gfx::Rect* bounds) const = 0;
  };

  // Determines the position, size and maximized state for a window as it is
  // created. This function uses several strategies to figure out optimal size
  // and placement, first looking for an existing active window, then falling
  // back to persisted data from a previous session, finally utilizing a default
  // algorithm. If |specified_bounds| are non-empty, this value is returned
  // instead. For use only in testing.
  //
  // NOTE: |maximized| is only set if we're restoring a saved maximized window.
  // When creating a new window based on an existing active window, standard
  // Windows behavior is to have it always be nonmaximized, even if the existing
  // window is maximized.
  void DetermineWindowBounds(const gfx::Rect& specified_bounds,
                             gfx::Rect* bounds,
                             bool* maximized) const;

  // Determines the size, position and maximized state for the browser window.
  // See documentation for DetermineWindowBounds below. Normally,
  // |window_bounds| is calculated by calling GetLastActiveWindowState(). To
  // explicitly specify a particular window to base the bounds on, pass in a
  // non-NULL value for |browser|.
  static void GetBrowserWindowBounds(const std::wstring& app_name,
                                     const gfx::Rect& specified_bounds,
                                     Browser* browser,
                                     gfx::Rect* window_bounds,
                                     bool* maximized);

  // Returns the default origin for popups of the given size.
  static gfx::Point GetDefaultPopupOrigin(const gfx::Size& size);

  // How much horizontal and vertical offset there is between newly
  // opened windows.  This value may be different on each platform.
  static const int kWindowTilePixels;

 private:
  // The edge of the screen to check for out-of-bounds.
  enum Edge { TOP, LEFT, BOTTOM, RIGHT };

  explicit WindowSizer(const std::wstring& app_name);

  void Init(StateProvider* state_provider,
            MonitorInfoProvider* monitor_info_provider);

  // Gets the size and placement of the last window. Returns true if this data
  // is valid, false if there is no last window and the application should
  // restore saved state from preferences using RestoreWindowPosition.
  bool GetLastWindowBounds(gfx::Rect* bounds) const;

  // Gets the size and placement of the last window in the last session, saved
  // in local state preferences. Returns true if local state exists containing
  // this information, false if this information does not exist and a default
  // size should be used.
  bool GetSavedWindowBounds(gfx::Rect* bounds, bool* maximized) const;

  // Gets the default window position and size if there is no last window and
  // no saved window placement in prefs. This function determines the default
  // size based on monitor size, etc.
  void GetDefaultWindowBounds(gfx::Rect* default_bounds) const;

  // Returns true if the specified position is "offscreen" for the given edge,
  // meaning that it's outside all work areas in the direction of that edge.
  bool PositionIsOffscreen(int position, Edge edge) const;

  // Adjusts |bounds| to be visible onscreen, biased toward the work area of the
  // monitor containing |other_bounds|.  Despite the name, this doesn't
  // guarantee the bounds are fully contained within this monitor's work rect;
  // it just tried to ensure the edges are visible on _some_ work rect.
  void AdjustBoundsToBeVisibleOnMonitorContaining(
      const gfx::Rect& other_bounds,
      gfx::Rect* bounds) const;

  // Providers for persistent storage and monitor metrics.
  StateProvider* state_provider_;
  MonitorInfoProvider* monitor_info_provider_;

  DISALLOW_EVIL_CONSTRUCTORS(WindowSizer);
};

#endif  // CHROME_BROWSER_WINDOW_SIZER_H_
