// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_button_gtk.h"

TabButtonGtk::TabButtonGtk(Delegate* delegate)
    : state_(BS_NORMAL),
      mouse_pressed_(false),
      delegate_(delegate) {
}

TabButtonGtk::~TabButtonGtk() {
}

bool TabButtonGtk::IsPointInBounds(const gfx::Point& point) {
  GdkRegion* region = delegate_->MakeRegionForButton(this);
  if (!region)
    return bounds_.Contains(point);

  bool in_bounds = (gdk_region_point_in(region, point.x(), point.y()) == TRUE);
  gdk_region_destroy(region);
  return in_bounds;
}

bool TabButtonGtk::OnMotionNotify(GdkEventMotion* event) {
  ButtonState state;

  gfx::Point point(event->x, event->y);
  if (IsPointInBounds(point)) {
    if (mouse_pressed_) {
      state = BS_PUSHED;
    } else {
      state = BS_HOT;
    }
  } else {
    state = BS_NORMAL;
  }

  bool need_redraw = (state_ != state);
  state_ = state;
  return need_redraw;
}

bool TabButtonGtk::OnMousePress() {
  if (state_ == BS_HOT) {
    mouse_pressed_ = true;
    state_ = BS_PUSHED;
    return true;
  }

  return false;
}

void TabButtonGtk::OnMouseRelease() {
  mouse_pressed_ = false;

  if (state_ == BS_PUSHED) {
    delegate_->OnButtonActivate(this);

    // Jiggle the mouse so we re-highlight the Tab button.
    HighlightTabButton();
  }

  state_ = BS_NORMAL;
}

bool TabButtonGtk::OnLeaveNotify() {
  bool paint = (state_ != BS_NORMAL);
  state_ = BS_NORMAL;
  return paint;
}

void TabButtonGtk::Paint(ChromeCanvasPaint* canvas) {
  canvas->DrawBitmapInt(images_[state_], bounds_.x(), bounds_.y());
}

void TabButtonGtk::SetImage(ButtonState state, SkBitmap* bitmap) {
  images_[state] = bitmap ? *bitmap : SkBitmap();
}

void TabButtonGtk::HighlightTabButton() {
  // Get default display and screen.
  GdkDisplay* display = gdk_display_get_default();
  GdkScreen* screen = gdk_display_get_default_screen(display);

  // Get cursor position.
  int x, y;
  gdk_display_get_pointer(display, NULL, &x, &y, NULL);

  // Reset cusor position.
  gdk_display_warp_pointer(display, screen, x, y);
}
