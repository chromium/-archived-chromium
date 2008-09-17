// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/non_client_view.h"

namespace ChromeViews {

int NonClientView::GetHTComponentForFrame(const gfx::Point& point,
                                          int resize_area_size,
                                          int resize_area_corner_size,
                                          int top_resize_area_size,
                                          bool can_resize) {
  int component = HTNOWHERE;
  if (point.x() < resize_area_size) {
    if (point.y() < resize_area_corner_size) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height() - resize_area_corner_size)) {
      component = HTBOTTOMLEFT;
    } else {
      component = HTLEFT;
    }
  } else if (point.x() < resize_area_corner_size) {
    if (point.y() < top_resize_area_size) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height() - resize_area_size)) {
      component = HTBOTTOMLEFT;
    }
  } else if (point.x() >= (width() - resize_area_size)) {
    if (point.y() < resize_area_corner_size) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height() - resize_area_corner_size)) {
      component = HTBOTTOMRIGHT;
    } else if (point.x() >= (width() - resize_area_size)) {
      component = HTRIGHT;
    }
  } else if (point.x() >= (width() - resize_area_corner_size)) {
    if (point.y() < top_resize_area_size) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height() - resize_area_size)) {
      component = HTBOTTOMRIGHT;
    }
  } else if (point.y() < top_resize_area_size) {
    component = HTTOP;
  } else if (point.y() >= (height() - resize_area_size)) {
    component = HTBOTTOM;
  }

  // If the window can't be resized, there are no resize boundaries, just
  // window borders.
  if (component != HTNOWHERE)
    return can_resize ? component : HTBORDER;
  return HTNOWHERE;
}

}  // namespace ChromeViews

