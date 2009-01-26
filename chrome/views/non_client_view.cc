// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/non_client_view.h"

namespace views {

int NonClientView::GetHTComponentForFrame(const gfx::Point& point,
                                          int top_resize_border_height,
                                          int resize_border_width,
                                          int bottom_resize_border_height,
                                          int resize_corner_size,
                                          bool can_resize) {
  int component = HTNOWHERE;
  if (point.x() < resize_border_width) {
    if (point.y() < resize_corner_size) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height() - resize_corner_size)) {
      component = HTBOTTOMLEFT;
    } else {
      component = HTLEFT;
    }
  } else if (point.x() < resize_corner_size) {
    if (point.y() < top_resize_border_height) {
      component = HTTOPLEFT;
    } else if (point.y() >= (height() - bottom_resize_border_height)) {
      component = HTBOTTOMLEFT;
    }
  } else if (point.x() >= (width() - resize_border_width)) {
    if (point.y() < resize_corner_size) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height() - resize_corner_size)) {
      component = HTBOTTOMRIGHT;
    } else {
      component = HTRIGHT;
    }
  } else if (point.x() >= (width() - resize_corner_size)) {
    if (point.y() < top_resize_border_height) {
      component = HTTOPRIGHT;
    } else if (point.y() >= (height() - bottom_resize_border_height)) {
      component = HTBOTTOMRIGHT;
    }
  } else if (point.y() < top_resize_border_height) {
    component = HTTOP;
  } else if (point.y() >= (height() - bottom_resize_border_height)) {
    component = HTBOTTOM;
  }

  // If the window can't be resized, there are no resize boundaries, just
  // window borders.
  if (component != HTNOWHERE)
    return can_resize ? component : HTBORDER;
  return HTNOWHERE;
}

}  // namespace views

