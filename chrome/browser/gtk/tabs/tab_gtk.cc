// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_gtk.h"

#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;


///////////////////////////////////////////////////////////////////////////////
// TabGtk, public:

TabGtk::TabGtk(TabDelegate* delegate)
    : TabRendererGtk(),
      delegate_(delegate),
      closing_(false),
      mouse_pressed_(false) {
}

TabGtk::~TabGtk() {
}

bool TabGtk::IsPointInBounds(const gfx::Point& point) {
  GdkRegion* region = MakeRegionForTab();
  bool in_bounds = (gdk_region_point_in(region, point.x(), point.y()) == TRUE);
  gdk_region_destroy(region);
  return in_bounds;
}

bool TabGtk::OnMotionNotify(const gfx::Point& point) {
  CloseButtonState state;
  if (close_button_bounds().Contains(point)) {
    if (mouse_pressed_) {
      state = BS_PUSHED;
    } else {
      state = BS_HOT;
    }
  } else {
    state = BS_NORMAL;
  }

  bool need_redraw = (close_button_state() != state);
  set_close_button_state(state);
  return need_redraw;
}

bool TabGtk::OnMousePress() {
  if (close_button_state() == BS_HOT) {
    mouse_pressed_ = true;
    set_close_button_state(BS_PUSHED);
    return true;
  }

  return false;
}

void TabGtk::OnMouseRelease() {
  mouse_pressed_ = false;

  if (close_button_state() == BS_PUSHED)
    delegate_->CloseTab(this);
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, TabRendererGtk overrides:

bool TabGtk::IsSelected() const {
  return delegate_->IsTabSelected(this);
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, private:

GdkRegion* TabGtk::MakeRegionForTab()const {
  int w = width();
  int h = height();
  static const int kNumRegionPoints = 9;

  GdkPoint polygon[kNumRegionPoints] = {
    { 0, h },
    { kTabBottomCurveWidth, h - kTabBottomCurveWidth },
    { kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth },
    { kTabCapWidth, 0 },
    { w - kTabCapWidth, 0 },
    { w - kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth },
    { w - kTabBottomCurveWidth, h - kTabBottomCurveWidth },
    { w, h },
    { 0, h },
  };

  GdkRegion* region = gdk_region_polygon(polygon, kNumRegionPoints,
                                         GDK_WINDING_RULE);
  gdk_region_offset(region, x(), y());
  return region;
}
