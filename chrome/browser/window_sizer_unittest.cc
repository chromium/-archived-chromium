// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/window_sizer.h"
#include "testing/gtest/include/gtest/gtest.h"

// Some standard monitor sizes (no task bar).
static const gfx::Rect tentwentyfour(0, 0, 1024, 768);
static const gfx::Rect twelveeighty(0, 0, 1280, 1024);
static const gfx::Rect sixteenhundred(0, 0, 1600, 1200);
static const gfx::Rect sixteeneighty(0, 0, 1680, 1050);
static const gfx::Rect nineteentwenty(0, 0, 1920, 1200);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate left of the primary 1024x768 monitor.
static const gfx::Rect left_nonprimary(-1024, 0, 1024, 768);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate top of the primary 1024x768 monitor.
static const gfx::Rect top_nonprimary(0, -768, 1024, 768);

// The working area for 1024x768 monitors with different taskbar orientations.
static const gfx::Rect taskbar_bottom_work_area(0, 0, 1024, 734);
static const gfx::Rect taskbar_top_work_area(0, 34, 1024, 734);
static const gfx::Rect taskbar_left_work_area(107, 0, 917, 768);
static const gfx::Rect taskbar_right_work_area(0, 0, 917, 768);

// Testing implementation of WindowSizer::MonitorInfoProvider that we can use
// to fake various monitor layouts and sizes.
class TestMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  TestMonitorInfoProvider() {}
  virtual ~TestMonitorInfoProvider() {}

  void AddMonitor(const gfx::Rect& bounds, const gfx::Rect& work_rect) {
    DCHECK(bounds.Contains(work_rect));
    monitor_bounds_.push_back(work_rect);
    monitor_work_rects_.push_back(work_rect);
  }

  // Overridden from WindowSizer::MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryMonitorWorkingRect() const {
    return monitor_work_rects_[0];
  }
  virtual gfx::Rect GetPrimaryMonitorBounds() const {
    return monitor_bounds_[0];
  }
  virtual gfx::Rect GetMonitorWorkingRectMatching(
      const gfx::Rect& match_rect) const {
    return GetWorkingRectAt(GetMonitorIndexMatchingBounds(match_rect));
  }
  virtual gfx::Point GetBoundsOffsetMatching(
      const gfx::Rect& match_rect) const {
    int monitor_index = GetMonitorIndexMatchingBounds(match_rect);
    gfx::Rect bounds = monitor_bounds_[monitor_index];
    gfx::Rect work_rect = monitor_work_rects_[monitor_index];
    return gfx::Point(work_rect.x() - bounds.x(), work_rect.y() - bounds.y());
  }
  virtual int GetMonitorCount() const {
    return static_cast<int>(monitor_work_rects_.size());
  }
  virtual gfx::Rect GetWorkingRectAt(int index) const {
    DCHECK(index >= 0 && index < GetMonitorCount());
    return monitor_work_rects_.at(index);
  }

 private:
  int GetMonitorIndexMatchingBounds(const gfx::Rect& match_rect) const {
    int monitor_count = GetMonitorCount();
    int max_area = 0;
    int max_area_index = 0;
    // Loop through all the monitors, finding the one that intersects the
    // largest area of the supplied match rect.
    for (int i = 0; i < monitor_count; ++i) {
      gfx::Rect current_rect = GetWorkingRectAt(i);
      if (match_rect.right() < current_rect.right() &&
          match_rect.right() >= current_rect.x() &&
          match_rect.bottom() < current_rect.bottom() &&
          match_rect.bottom() >= current_rect.y()) {
        int covered_width, covered_height;
        if (match_rect.x() < current_rect.x()) {
          covered_width = match_rect.right() - current_rect.x();
        } else if (match_rect.right() > current_rect.right()) {
          covered_width = current_rect.right() - match_rect.x();
        } else {
          covered_width = match_rect.width();
        }
        if (match_rect.y() < current_rect.y()) {
          covered_height = match_rect.bottom() - current_rect.y();
        } else if (match_rect.bottom() > current_rect.bottom()) {
          covered_height = current_rect.bottom() - match_rect.y();
        } else {
          covered_height = match_rect.height();
        }
        int area = covered_width * covered_height;
        if (area > max_area) {
          max_area = area;
          max_area_index = i;
        }
      }
    }
    return max_area_index;
  }

  std::vector<gfx::Rect> monitor_bounds_;
  std::vector<gfx::Rect> monitor_work_rects_;

  DISALLOW_EVIL_CONSTRUCTORS(TestMonitorInfoProvider);
};

// Testing implementation of WindowSizer::StateProvider that we use to fake
// persistent storage and existing windows.
class TestStateProvider : public WindowSizer::StateProvider {
 public:
  TestStateProvider()
    : persistent_maximized_(false),
      has_persistent_data_(false),
      has_last_active_data_(false) {
  }
  virtual ~TestStateProvider() {}

  void SetPersistentState(const gfx::Rect& bounds,
                          bool maximized,
                          bool has_persistent_data) {
    persistent_bounds_ = bounds;
    persistent_maximized_ = maximized;
    has_persistent_data_ = has_persistent_data;
  }

  void SetLastActiveState(const gfx::Rect& bounds, bool has_last_active_data) {
    last_active_bounds_ = bounds;
    has_last_active_data_ = has_last_active_data;
  }

  // Overridden from WindowSizer::StateProvider:
  virtual bool GetPersistentState(gfx::Rect* bounds, bool* maximized) const {
    *bounds = persistent_bounds_;
    *maximized = persistent_maximized_;
    return has_persistent_data_;
  }

  virtual bool GetLastActiveWindowState(gfx::Rect* bounds) const {
    *bounds = last_active_bounds_;
    return has_last_active_data_;
  }

 private:
  gfx::Rect persistent_bounds_;
  bool persistent_maximized_;
  bool has_persistent_data_;

  gfx::Rect last_active_bounds_;
  bool has_last_active_data_;

  DISALLOW_EVIL_CONSTRUCTORS(TestStateProvider);
};

// A convenience function to read the window bounds from the window sizer
// according to the specified configuration.
enum Source { DEFAULT, LAST_ACTIVE, PERSISTED };
static void GetWindowBounds(const gfx::Rect& monitor1_bounds,
                            const gfx::Rect& monitor1_work_rect,
                            const gfx::Rect& monitor2_bounds,
                            const gfx::Rect& state,
                            bool maximized,
                            Source source,
                            gfx::Rect* out_bounds,
                            bool* out_maximized) {
  TestMonitorInfoProvider* mip = new TestMonitorInfoProvider;
  mip->AddMonitor(monitor1_bounds, monitor1_work_rect);
  if (!monitor2_bounds.IsEmpty())
    mip->AddMonitor(monitor2_bounds, monitor2_bounds);
  TestStateProvider* sp = new TestStateProvider;
  if (source == PERSISTED)
    sp->SetPersistentState(state, maximized, true);
  else if (source == LAST_ACTIVE)
    sp->SetLastActiveState(state, true);
  WindowSizer sizer(sp, mip);
  sizer.DetermineWindowBounds(gfx::Rect(), out_bounds, out_maximized);
}

// Test that the window is sized appropriately for the first run experience
// where the default window bounds calculation is invoked.
TEST(WindowSizerTest, DefaultSizeCase) {

  { // 4:3 monitor case, 1024x768, no taskbar
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(), gfx::Rect(),
                    false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 1004, 748), window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on bottom
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, taskbar_bottom_work_area, gfx::Rect(),
                    gfx::Rect(), false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 1004, 714), window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on right
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, taskbar_right_work_area, gfx::Rect(),
                    gfx::Rect(), false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 897, 748), window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on left
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, taskbar_left_work_area, gfx::Rect(),
                    gfx::Rect(), false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(117, 10, 897, 748), window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on top
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, taskbar_top_work_area, gfx::Rect(),
                    gfx::Rect(), false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 44, 1004, 714), window_bounds);
  }

  { // 4:3 monitor case, 1280x1024
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(twelveeighty, twelveeighty, gfx::Rect(), gfx::Rect(),
                    false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 1050, 1004), window_bounds);
  }

  { // 4:3 monitor case, 1600x1200
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(sixteenhundred, sixteenhundred, gfx::Rect(), gfx::Rect(),
                    false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 1050, 1180), window_bounds);
  }

  { // 16:10 monitor case, 1680x1050
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(sixteeneighty, sixteeneighty, gfx::Rect(), gfx::Rect(),
                    false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 810, 1030), window_bounds);
  }

  { // 16:10 monitor case, 1920x1200
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(nineteentwenty, nineteentwenty, gfx::Rect(), gfx::Rect(),
                    false, DEFAULT, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 930, 1180), window_bounds);
  }
}

// Test that the next opened window is positioned appropriately given the
// bounds of an existing window of the same type.
TEST(WindowSizerTest, LastWindowBoundsCase) {

  { // normal, in the middle of the screen somewhere.
    gfx::Rect window_bounds;
    bool maximized = false;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(10, 10, 500, 400), false, LAST_ACTIVE,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(20, 20, 500, 400), window_bounds);
  }

  { // normal, but maximized
    gfx::Rect window_bounds;
    bool maximized = false;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(10, 10, 500, 400), true, LAST_ACTIVE,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(20, 20, 500, 400), window_bounds);
  }

  { // offset would put the new window offscreen at the bottom
    gfx::Rect window_bounds;
    bool maximized = false;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(10, 360, 500, 400), false, LAST_ACTIVE,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(20, 358, 500, 400), window_bounds);
  }

  { // offset would put the new window offscreen at the right
    gfx::Rect window_bounds;
    bool maximized = false;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(520, 10, 500, 400), false, LAST_ACTIVE,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(514, 20, 500, 400), window_bounds);
  }

  { // offset would put the new window offscreen at the bottom right
    gfx::Rect window_bounds;
    bool maximized = false;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(520, 360, 500, 400), false, LAST_ACTIVE,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(514, 358, 500, 400), window_bounds);
  }
}

// Test that the window opened is sized appropriately given persisted sizes.
TEST(WindowSizerTest, PersistedBoundsCase) {

  { // normal, in the middle of the screen somewhere.
    gfx::Rect initial_bounds(10, 10, 500, 400);

    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(), initial_bounds,
                    false, PERSISTED, &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(initial_bounds, window_bounds);
  }

  { // normal, maximized.
    gfx::Rect initial_bounds(0, 0, 1024, 768);

    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(), initial_bounds,
                    true, PERSISTED, &window_bounds, &maximized);
    EXPECT_TRUE(maximized);
    EXPECT_EQ(initial_bounds, window_bounds);
  }

  { // normal, on non-primary monitor in negative coords.
    gfx::Rect initial_bounds(-600, 10, 500, 400);

    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, left_nonprimary,
                    initial_bounds, false, PERSISTED, &window_bounds,
                    &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(initial_bounds, window_bounds);
  }

  { // normal, on non-primary monitor in negative coords, maximized.
    gfx::Rect initial_bounds(-1024, 0, 1024, 768);

    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, left_nonprimary,
                    initial_bounds, true, PERSISTED, &window_bounds,
                    &maximized);
    EXPECT_TRUE(maximized);
    EXPECT_EQ(initial_bounds, window_bounds);
  }

  { // a little off the left
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(-10, 50, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 50, 500, 400), window_bounds);
  }

  { // a little off the top
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(50, -10, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(50, 10, 500, 400), window_bounds);
  }

  { // a little off the right
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(534, 50, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(514, 50, 500, 400), window_bounds);
  }

  { // a little off the bottom
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(50, 378, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(50, 358, 500, 400), window_bounds);
  }

  { // a little off the topleft
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(-10, -10, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 500, 400), window_bounds);
  }

  { // a little off the topright
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(534, -10, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(514, 10, 500, 400), window_bounds);
  }

  { // a little off the bottomleft
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(-10, 378, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 358, 500, 400), window_bounds);
  }

  { // a little off the bottomright
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(534, 378, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(514, 358, 500, 400), window_bounds);
  }

  { // split across two, bias right
    gfx::Rect initial_bounds(-50, 50, 500, 400);
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, left_nonprimary,
                    initial_bounds, false, PERSISTED, &window_bounds,
                    &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(initial_bounds, window_bounds);
  }

  { // split across two, bias left
    gfx::Rect initial_bounds(-450, 50, 500, 400);
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, left_nonprimary,
                    initial_bounds, false, PERSISTED, &window_bounds,
                    &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(initial_bounds, window_bounds);
  }

  { // split across two, a little off left
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, top_nonprimary,
                    gfx::Rect(-50, -50, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, -50, 500, 400), window_bounds);
  }

  { // split across two, a little off right
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, top_nonprimary,
                    gfx::Rect(534, -50, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(514, -50, 500, 400), window_bounds);
  }

  { // split across two, a little off top
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, left_nonprimary,
                    gfx::Rect(-50, -50, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(-50, 10, 500, 400), window_bounds);
  }

  { // split across two, a little off bottom
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, left_nonprimary,
                    gfx::Rect(-50, 378, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(-50, 358, 500, 400), window_bounds);
  }

  { // entirely off left (monitor was detached since last run)
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(-700, 50, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 50, 500, 400), window_bounds);
  }

  { // entirely off top (monitor was detached since last run)
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(50, -500, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(50, 10, 500, 400), window_bounds);
  }

  { // entirely off right (monitor was detached since last run)
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(1200, 50, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(514, 50, 500, 400), window_bounds);
  }

  { // entirely off bottom (monitor was detached since last run)
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(50, 800, 500, 400), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(50, 358, 500, 400), window_bounds);
  }

  { // width and height too large (monitor screen resolution changed since last
    // run)
    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, tentwentyfour, gfx::Rect(),
                    gfx::Rect(10, 10, 1200, 900), false, PERSISTED,
                    &window_bounds, &maximized);
    EXPECT_FALSE(maximized);
    EXPECT_EQ(gfx::Rect(10, 10, 1004, 748), window_bounds);
  }

  { // Handles taskbar offset on the top.
    gfx::Rect initial_bounds(10, 10, 500, 400);

    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, taskbar_top_work_area, gfx::Rect(),
                    initial_bounds, false, PERSISTED, &window_bounds,
                    &maximized);
    EXPECT_FALSE(maximized);
    initial_bounds.Offset(taskbar_top_work_area.x(), taskbar_top_work_area.y());
    EXPECT_EQ(initial_bounds, window_bounds);
  }

  { // Handles taskbar offset on the left.
    gfx::Rect initial_bounds(10, 10, 500, 400);

    gfx::Rect window_bounds;
    bool maximized;
    GetWindowBounds(tentwentyfour, taskbar_left_work_area, gfx::Rect(),
                    initial_bounds, false, PERSISTED, &window_bounds,
                    &maximized);
    EXPECT_FALSE(maximized);
    initial_bounds.Offset(taskbar_left_work_area.x(),
                          taskbar_left_work_area.y());
    EXPECT_EQ(initial_bounds, window_bounds);
  }
}
