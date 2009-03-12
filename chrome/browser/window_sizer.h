// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WINDOW_SIZER_H__
#define CHROME_BROWSER_WINDOW_SIZER_H__

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

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

  // An interface implemented by an object that can retrieve information about
  // the monitors on the system.
  class MonitorInfoProvider {
   public:
    virtual ~MonitorInfoProvider() { }

    // Returns the bounds of the working rect of the primary monitor.
    virtual gfx::Rect GetPrimaryMonitorWorkingRect() const = 0;

    // Returns the bounds of the entire primary screen.
    virtual gfx::Rect GetPrimaryMonitorBounds() const = 0;

    // Returns the bounds of the working rect of the monitor that most closely
    // intersects the provided bounds.
    virtual gfx::Rect GetMonitorWorkingRectMatching(
        const gfx::Rect& match_rect) const = 0;

    // Returns the delta between the working rect and the monitor size of the
    // monitor that most closely intersects the provided bounds.
    virtual gfx::Point GetBoundsOffsetMatching(
        const gfx::Rect& match_rect) const = 0;

    // Returns the number of monitors on the system.
    virtual int GetMonitorCount() const = 0;

    // Returns the bounds of the working rect of the monitor at the specified
    // index.
    virtual gfx::Rect GetWorkingRectAt(int index) const = 0;
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

  // Determines the size, position and maximized state for the browser window.
  // See documentation for DetermineWindowBounds below.
  static void GetBrowserWindowBounds(const std::wstring& app_name,
                                     const gfx::Rect& specified_bounds,
                                     gfx::Rect* window_bounds,
                                     bool* maximized);

  // Returns the default origin for popups of the given size.
  static gfx::Point GetDefaultPopupOrigin(const gfx::Size& size);

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

 private:
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

  // The edge of the work area to check for out-of-bounds.
  enum Edge { TOP, LEFT, BOTTOM, RIGHT };

  // Return true if the position is over the specified edge on the valid
  // monitor work are rects.
  bool PositionIsOffscreen(int position, Edge edge) const;

  // Adjust the bounds specified in |bounds| so that they fit completely within
  // the work area of the monitor that contains |other_bounds|.
  void AdjustBoundsToBeVisibleOnMonitorContaining(
      const gfx::Rect& other_bounds,
      gfx::Rect* bounds /* inout */) const;

  // Providers for persistent storage and monitor metrics.
  StateProvider* state_provider_;
  MonitorInfoProvider* monitor_info_provider_;

  DISALLOW_EVIL_CONSTRUCTORS(WindowSizer);
};


#endif  // #ifndef CHROME_BROWSER_WINDOW_SIZER_H__
